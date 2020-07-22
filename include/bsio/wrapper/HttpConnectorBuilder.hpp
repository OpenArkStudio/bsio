#pragma once

#include <bsio/wrapper/internal/Option.hpp>
#include <bsio/wrapper/internal/HttpSessionBuilder.hpp>

namespace bsio { namespace net { namespace wrapper {

    class HttpConnectorBuilder : public internal::BaseHttpSessionBuilder<HttpConnectorBuilder>
    {
    public:
        virtual ~HttpConnectorBuilder() = default;

        HttpConnectorBuilder& WithConnector(TcpConnector connector) noexcept
        {
            mConnector = std::move(connector);
            return *this;
        }

        HttpConnectorBuilder& WithEndpoint(asio::ip::tcp::endpoint endpoint) noexcept
        {
            mSocketOption.endpoint = std::move(endpoint);
            return *this;
        }

        HttpConnectorBuilder& WithTimeout(std::chrono::nanoseconds timeout) noexcept
        {
            mSocketOption.timeout = timeout;
            return *this;
        }

        HttpConnectorBuilder& WithFailedHandler(SocketFailedConnectHandler handler) noexcept
        {
            mSocketOption.failedHandler = std::move(handler);
            return *this;
        }

        HttpConnectorBuilder& AddSocketProcessingHandler(SocketProcessingHandler handler) noexcept
        {
            mSocketOption.socketProcessingHandlers.push_back(std::move(handler));
            return *this;
        }

        void asyncConnect()
        {
            if (!mConnector)
            {
                throw std::runtime_error("connector is empty");
            }

            setupHttp();

            mConnector->asyncConnect(
                mSocketOption.endpoint,
                mSocketOption.timeout,
                mSocketOption.establishHandler,
                mSocketOption.failedHandler,
                mSocketOption.socketProcessingHandlers);
        }

    private:
        void setupHttp()
        {
            mSocketOption.establishHandler = [copy = *this](asio::ip::tcp::socket socket)
            {
                internal::setupHttpSession(std::move(socket),
                                           copy.SessionOption(),
                                           copy.EnterCallback(),
                                           copy.ParserCallback(),
                                           copy.WsCallback());
            };
        }

    private:
        std::optional<TcpConnector>             mConnector;
        internal::SocketConnectOption           mSocketOption;
    };

} } }