#pragma once

#include <bsio/Functor.hpp>
#include <bsio/wrapper/internal/Option.hpp>
#include <bsio/http/HttpService.hpp>

namespace bsio { namespace net { namespace wrapper { namespace common {

    class HttpSessionBuilder
    {
    public:
        auto& WithEnterCallback(http::HttpSession::EnterCallback callback) noexcept
        {
            mEnterCallback = std::move(callback);
            return *this;
        }

        auto& WithParserCallback(http::HttpSession::HttpParserCallback callback) noexcept
        {
            mParserCallback = std::move(callback);
            return *this;
        }

        auto& WithWsCallback(http::HttpSession::WsCallback handler) noexcept
        {
            mWsCallback = std::move(handler);
            return *this;
        }

        auto& WithRecvBufferSize(size_t size) noexcept
        {
            mTcpSessionOption.recvBufferSize = size;
            return *this;
        }

        auto&   WithCloseCallback(TcpSession::ClosedHandler handler) noexcept
        {
            mTcpSessionOption.closedHandler = std::move(handler);
            return *this;
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

    using HttpSessionBuilderCallback = std::function<void(HttpSessionBuilder&)>;

    inline SocketEstablishHandler generateHttpEstablishHandler(
            HttpSessionBuilderCallback callback)
    {
        return [callback = std::move(callback)](asio::ip::tcp::socket socket)
        {
            HttpSessionBuilder builder;
            callback(builder);

            const auto session = TcpSession::Make(std::move(socket),
                                                  builder.SessionOption().recvBufferSize,
                                                  nullptr,
                                                  builder.SessionOption().closedHandler);
            auto httpSession = std::make_shared<http::HttpSession>(
                    session,
                    builder.ParserCallback(),
                    builder.WsCallback(),
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

            if (builder.EnterCallback() != nullptr)
            {
                builder.EnterCallback()(httpSession);
            }
        };
    }

} } } }