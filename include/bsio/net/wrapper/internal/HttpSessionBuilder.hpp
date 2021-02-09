#pragma once

#include <bsio/net/Functor.hpp>
#include <bsio/net/http/HttpService.hpp>
#include <bsio/net/wrapper/internal/Option.hpp>

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

// TODO
void setupHttpSession(TcpSession::Ptr session,
                      const http::HttpSession::EnterCallback& httpEnterCallback,
                      const http::HttpSession::HttpParserCallback& httpParserCallback,
                      const http::HttpSession::WsCallback& httpWsCallback,
                      const http::HttpSession::ClosedCallback& closedCallback)
{
    auto httpSession = std::make_shared<http::HttpSession>(
            session,
            httpParserCallback,
            httpWsCallback,
            nullptr,//TODO
            closedCallback);

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
        else
        {
            retLen = http::HttpService::ProcessHttp(reader.begin(),
                                                    reader.size(),
                                                    httpParser,
                                                    httpSession);
        }

        reader.addPos(retLen);
        reader.savePos();
    };

    session->asyncSetDataHandler(dataHandler);
    session->asyncSetClosedHandler([=](const TcpSession::Ptr& session) {
        closedCallback(httpSession);
    });

    if (httpEnterCallback != nullptr)
    {
        httpEnterCallback(httpSession);
    }
}

}// namespace bsio::net::wrapper::internal
