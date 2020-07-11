#pragma once

#include <bsio/Functor.hpp>
#include <bsio/wrapper/internal/Option.hpp>
#include <bsio/http/HttpService.hpp>

namespace bsio { namespace net { namespace wrapper { namespace common {

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

        Derived& WithRecvBufferSize(size_t size) noexcept
        {
            mTcpSessionOption.recvBufferSize = size;
            return static_cast<Derived&>(*this);
        }

        Derived&   WithCloseCallback(TcpSession::ClosedHandler handler) noexcept
        {
            mTcpSessionOption.closedHandler = std::move(handler);
            return static_cast<Derived&>(*this);
        }

        const auto& SessionOption() const
        {
            return mTcpSessionOption;
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

    private:
        internal::TcpSessionOption              mTcpSessionOption;

        http::HttpSession::EnterCallback        mEnterCallback;
        http::HttpSession::HttpParserCallback   mParserCallback;
        http::HttpSession::WsCallback           mWsCallback;
    };

    class HttpSessionBuilder : public BaseHttpSessionBuilder<HttpSessionBuilder>, public asio::noncopyable
    {};

    using HttpSessionBuilderCallback = std::function<void(HttpSessionBuilder&)>;

    void initHttpSession(asio::ip::tcp::socket socket, const internal::TcpSessionOption& option,
              const http::HttpSession::EnterCallback&        httpEnterCallback,
              const http::HttpSession::HttpParserCallback&   httpParserCallback,
              const http::HttpSession::WsCallback&           httpWsCallback)
    {
        const auto session = TcpSession::Make(std::move(socket),
                                              option.recvBufferSize,
                                              nullptr,
                                              option.closedHandler);
        auto httpSession = std::make_shared<http::HttpSession>(
                session,
                httpParserCallback,
                httpWsCallback,
                nullptr,
                nullptr);

        auto httpParser = std::make_shared<http::HTTPParser>(HTTP_BOTH);
        auto dataHandler = [=](const TcpSession::Ptr& session, const char* buffer, size_t len)
        {
            (void)session;

            if (httpParser->isWebSocket())
            {
                return http::HttpService::ProcessWebSocket(buffer,
                                                           len,
                                                           httpParser,
                                                           httpSession);
            }
            return http::HttpService::ProcessHttp(buffer, len, httpParser, httpSession);
        };

        session->asyncSetDataHandler(dataHandler);

        if (httpEnterCallback != nullptr)
        {
            httpEnterCallback(httpSession);
        }
    }

    inline SocketEstablishHandler generateHttpEstablishHandler(
            HttpSessionBuilderCallback callback)
    {
        return [callback = std::move(callback)](asio::ip::tcp::socket socket)
        {
            HttpSessionBuilder builder;
            callback(builder);
            initHttpSession(std::move(socket),
                    builder.SessionOption(),
                    builder.EnterCallback(),
                    builder.ParserCallback(),
                    builder.WsCallback());
        };
    }

} } } }