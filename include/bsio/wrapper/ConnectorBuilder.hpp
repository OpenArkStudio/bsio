#pragma once

#include <memory>
#include <functional>

#include <bsio/TcpSession.hpp>
#include <bsio/wrapper/internal/Option.hpp>
#include <bsio/wrapper/internal/TcpSessionBuilder.hpp>

namespace bsio { namespace net { namespace wrapper {

    class TcpSessionConnectorBuilder : public internal::BaseSessionOptionBuilder<TcpSessionConnectorBuilder>
    {
    public:
        virtual ~TcpSessionConnectorBuilder() = default;

        TcpSessionConnectorBuilder& WithConnector(TcpConnector::Ptr connector) noexcept
        {
            mConnector = std::move(connector);
            return *this;
        }

        TcpSessionConnectorBuilder& WithEndpoint(asio::ip::tcp::endpoint endpoint) noexcept
        {
            mSocketOption.endpoint = std::move(endpoint);
            return *this;
        }

        TcpSessionConnectorBuilder& WithTimeout(std::chrono::nanoseconds timeout) noexcept
        {
            mSocketOption.timeout = timeout;
            return *this;
        }

        TcpSessionConnectorBuilder& WithFailedHandler(SocketFailedConnectHandler handler) noexcept
        {
            mSocketOption.failedHandler = std::move(handler);
            return *this;
        }

        TcpSessionConnectorBuilder& AddSocketProcessingHandler(SocketProcessingHandler handler) noexcept
        {
            mSocketOption.socketProcessingHandlers.push_back(std::move(handler));
            return *this;
        }

        void asyncConnect()
        {
            if (mConnector == nullptr)
            {
                throw std::runtime_error("connector is nullptr");
            }
            if (Option().dataHandler == nullptr)
            {
                throw std::runtime_error("data handler not setting");
            }

            mSocketOption.establishHandler =
                [option = Option()](asio::ip::tcp::socket socket)
            {
                const auto session = TcpSession::Make(
                    std::move(socket),
                    option.recvBufferSize,
                    option.dataHandler,
                    option.closedHandler);
                for (const auto& callback : option.establishHandlers)
                {
                    callback(session);
                }
            };

            mConnector->asyncConnect(
                mSocketOption.endpoint,
                mSocketOption.timeout,
                mSocketOption.establishHandler,
                mSocketOption.failedHandler,
                mSocketOption.socketProcessingHandlers);
        }

    private:
        TcpConnector::Ptr               mConnector;
        internal::SocketConnectOption   mSocketOption;
    };

} } }