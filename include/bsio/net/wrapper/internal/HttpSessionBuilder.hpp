#pragma once

#include <bsio/net/Functor.hpp>
#include <bsio/net/http/HttpService.hpp>
#include <bsio/net/wrapper/internal/Option.hpp>
#include <tuple>

namespace bsio::net::wrapper::internal {

template<typename Derived>
class BaseHttpSessionBuilder
{
public:
    Derived& WithEnterCallback(http::HttpSession::EnterCallback callback) noexcept
    {
        mEnterCallback = std::move(callback);
        return static_cast<Derived&>(*this);
    }

    Derived& WithParserCallback(http::HttpSession::HttpParserCallback callback) noexcept
    {
        mParserCallback = std::move(callback);
        return static_cast<Derived&>(*this);
    }

    Derived& WithWsCallback(http::HttpSession::WsCallback handler) noexcept
    {
        mWsCallback = std::move(handler);
        return static_cast<Derived&>(*this);
    }

    Derived& WithCloseCallback(http::HttpSession::ClosedCallback handler) noexcept
    {
        mClosedCallback = std::move(handler);
        return static_cast<Derived&>(*this);
    }

    const auto& EnterCallback() const
    {
        return mEnterCallback;
    }

    const auto& ParserCallback() const
    {
        return mParserCallback;
    }

    const auto& WsCallback() const
    {
        return mWsCallback;
    }

    const auto& ClosedCallback() const
    {
        return mClosedCallback;
    }

private:
    http::HttpSession::EnterCallback mEnterCallback;
    http::HttpSession::HttpParserCallback mParserCallback;
    http::HttpSession::WsCallback mWsCallback;
    http::HttpSession::ClosedCallback mClosedCallback;
};

std::tuple<TcpSession::DataHandler, TcpSession::EofHandler, TcpSession::ClosedHandler> makeHttpHandlers(http::HttpSession::Ptr httpSession)
{
    auto httpParser = std::make_shared<http::HTTPParser>(HTTP_BOTH);

    auto dataHandler = [=](const TcpSession::Ptr& session, bsio::base::BasePacketReader& reader) {
        (void) session;

        size_t retLen = 0;


        if (httpParser->isWebSocket())
        {
            retLen = http::HttpService::ProcessWebSocket(reader.begin(),
                                                         reader.size(),
                                                         httpParser,
                                                         httpSession);
        }
        else if (httpParser->isUpgrade())
        {
            // TODO::not support other upgrade protocol
        }
        else
        {
            retLen = http::HttpService::ProcessHttp(reader.begin(),
                                                    reader.size(),
                                                    httpParser,
                                                    httpSession);
            // if http_parser_execute not consume all data that indicate cause error in parser.
            // so we need close connection.
            if (retLen != reader.size())
            {
                httpSession->close();
            }
        }

        reader.addPos(retLen);
        reader.savePos();
    };

    auto closedHandler = [=](const TcpSession::Ptr& session) {
        if (auto closedCallback = httpSession->getCloseCallback())
        {
            closedCallback(httpSession);
        }
    };

    auto eofHandler = [=](const TcpSession::Ptr& session) {
        // try pass EOF to http parser
        if (!httpParser->isCompleted())
        {
            http::HttpService::ProcessHttp(nullptr, 0, httpParser, httpSession);
        }
    };

    return {dataHandler, eofHandler, closedHandler};
}

}// namespace bsio::net::wrapper::internal
