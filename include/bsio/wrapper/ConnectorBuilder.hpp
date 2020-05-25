#pragma once

#include <memory>
#include <functional>

#include <bsio/TcpSession.hpp>
#include <bsio/wrapper/internal/Option.hpp>

namespace bsio { namespace net { namespace wrapper {

    class TcpSessionConnectorBuilder
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

        TcpSessionConnectorBuilder& WithRecvBufferSize(size_t size) noexcept
        {
            mTcpSessionOption.recvBufferSize = size;
            return *this;
        }

        TcpSessionConnectorBuilder& AddEnterCallback(TcpSessionEstablishHandler handler) noexcept
        {
            mTcpSessionOption.establishHandlers.push_back(std::move(handler));
            return *this;
        }

        TcpSessionConnectorBuilder& WithClosedHandler(TcpSession::ClosedHandler handler) noexcept
        {
            mTcpSessionOption.closedHandler = std::move(handler);
            return *this;
        }

        TcpSessionConnectorBuilder& WithDataHandler(TcpSession::DataHandler handler) noexcept
        {
            mTcpSessionOption.dataHandler = std::move(handler);
            return *this;
        }

        void asyncConnect()
        {
            if (mConnector == nullptr)
            {
                throw std::runtime_error("connector is nullptr");
            }
            if (mTcpSessionOption.dataHandler == nullptr)
            {
                throw std::runtime_error("data handler not setting");
            }

            mSocketOption.establishHandler =
                [option = mTcpSessionOption](asio::ip::tcp::socket socket)
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
        TcpConnector::Ptr mConnector;
        internal::SocketConnectOption mSocketOption;
        internal::TcpSessionOption mTcpSessionOption;
    };

} } }