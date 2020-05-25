#pragma once

#include <bsio/TcpAcceptor.hpp>
#include <bsio/wrapper/internal/Option.hpp>

namespace bsio { namespace net { namespace wrapper {

    class TcpSessionAcceptorBuilder
    {
    public:
        virtual ~TcpSessionAcceptorBuilder() = default;

        TcpSessionAcceptorBuilder& WithAcceptor(TcpAcceptor::Ptr acceptor) noexcept
        {
            mAcceptor = std::move(acceptor);
            return *this;
        }

        TcpSessionAcceptorBuilder& AddSocketProcessingHandler(SocketProcessingHandler handler) noexcept
        {
            mServerSocketOption.socketProcessingHandlers.push_back(std::move(handler));
            return *this;
        }

        TcpSessionAcceptorBuilder& WithRecvBufferSize(size_t size) noexcept
        {
            mTcpSessionOption.recvBufferSize = size;
            return *this;
        }

        TcpSessionAcceptorBuilder& AddEnterCallback(TcpSessionEstablishHandler handler) noexcept
        {
            mTcpSessionOption.establishHandlers.push_back(std::move(handler));
            return *this;
        }

        TcpSessionAcceptorBuilder& WithClosedHandler(TcpSession::ClosedHandler handler) noexcept
        {
            mTcpSessionOption.closedHandler = std::move(handler);
            return *this;
        }

        TcpSessionAcceptorBuilder& WithDataHandler(TcpSession::DataHandler handler) noexcept
        {
            mTcpSessionOption.dataHandler = std::move(handler);
            return *this;
        }

        void    start()
        {
            if (mAcceptor == nullptr)
            {
                throw std::runtime_error("acceptor is nullptr");
            }
            if (mTcpSessionOption.dataHandler == nullptr)
            {
                throw std::runtime_error("data handler not setting");
            }

            // setting establishHandlers
            mServerSocketOption.establishHandler =
                [option = mTcpSessionOption](asio::ip::tcp::socket socket)
            {
                const auto session = TcpSession::Make(std::move(socket),
                    option.recvBufferSize,
                    option.dataHandler,
                    option.closedHandler);
                for (const auto& callback : option.establishHandlers)
                {
                    callback(session);
                }
            };

            mAcceptor->startAccept([option = mServerSocketOption](asio::ip::tcp::socket socket)
                {
                    for (const auto& handler : option.socketProcessingHandlers)
                    {
                        handler(socket);
                    }
                    option.establishHandler(std::move(socket));
                });
        }

    private:
        TcpAcceptor::Ptr    mAcceptor;
        internal::ServerSocketOption mServerSocketOption;
        internal::TcpSessionOption  mTcpSessionOption;
    };

} } }