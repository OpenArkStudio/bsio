#pragma once

#include <memory>
#include <functional>

#include <bsio/IoContextThreadPool.hpp>
#include <bsio/Functor.hpp>
#include <bsio/SharedSocket.hpp>
#include <asio/basic_socket_acceptor.hpp>

namespace bsio { namespace net {

    class TcpAcceptor
    {
    public:
        TcpAcceptor(
            asio::io_context& listenContext,
            IoContextThreadPool::Ptr ioContextThreadPool,
            const asio::ip::tcp::endpoint& endpoint)
            :
            mIoContextThreadPool(std::move(ioContextThreadPool)),
            mAcceptor(std::make_shared<AcceptorGuard>(listenContext, endpoint))
        {
        }

        virtual ~TcpAcceptor() = default;

        void    startAccept(const SocketEstablishHandler& callback)
        {
            doAccept(callback);
        }

        void    close() const
        {
            mAcceptor->close();
        }

    private:
        void    doAccept(const SocketEstablishHandler& callback)
        {
            if (!mAcceptor->is_open())
            {
                return;
            }

            auto& ioContext = mIoContextThreadPool->pickIoContext();
            auto sharedSocket = SharedSocket::Make(asio::ip::tcp::socket(ioContext), ioContext);

            mAcceptor->async_accept(
                    sharedSocket->socket(),
                    [callback, sharedSocket, *this](std::error_code ec) mutable
                    {
                        if (!ec)
                        {
                            sharedSocket->context().post([=]()
                            {
                                callback(std::move(sharedSocket->socket()));
                            });
                        }
                        doAccept(callback);
                    });
        }

    private:
        IoContextThreadPool::Ptr                    mIoContextThreadPool;

        class AcceptorGuard : public asio::ip::tcp::acceptor
        {
        public:
            using asio::ip::tcp::acceptor::acceptor;
            ~AcceptorGuard()
            {
                close();
            }
        };
        std::shared_ptr<AcceptorGuard>    mAcceptor;
    };

} }