#pragma once

#include <asio/basic_socket_acceptor.hpp>
#include <bsio/Functor.hpp>
#include <bsio/IoContextThreadPool.hpp>
#include <bsio/SharedSocket.hpp>
#include <functional>
#include <memory>

namespace bsio::net
{

    class TcpAcceptor : public asio::noncopyable,
                        public std::enable_shared_from_this<TcpAcceptor>
    {
    public:
        using Ptr = std::shared_ptr<TcpAcceptor>;

        static Ptr Make(asio::io_context& listenContext,
                        const IoContextThreadPool::Ptr& ioContextThreadPool,
                        const asio::ip::tcp::endpoint& endpoint)
        {
            class make_shared_enabler : public TcpAcceptor
            {
            public:
                make_shared_enabler(asio::io_context& listenContext,
                                    const IoContextThreadPool::Ptr& ioContextThreadPool,
                                    const asio::ip::tcp::endpoint& endpoint)
                    : TcpAcceptor(listenContext, ioContextThreadPool, endpoint)
                {
                }
            };

            auto acceptor = std::make_shared<make_shared_enabler>(listenContext, ioContextThreadPool, endpoint);
            return std::static_pointer_cast<TcpAcceptor>(acceptor);
        }

        virtual ~TcpAcceptor()
        {
            close();
        }

        void startAccept(const SocketEstablishHandler& callback)
        {
            doAccept(callback);
        }

        void close()
        {
            mAcceptor.close();
        }

    private:
        TcpAcceptor(
                asio::io_context& listenContext,
                const IoContextThreadPool::Ptr& ioContextThreadPool,
                const asio::ip::tcp::endpoint& endpoint)
            : mIoContextThreadPool(ioContextThreadPool),
              mAcceptor(listenContext, endpoint)
        {
        }

        void doAccept(const SocketEstablishHandler& callback)
        {
            if (!mAcceptor.is_open())
            {
                return;
            }

            auto& ioContext = mIoContextThreadPool->pickIoContext();
            auto sharedSocket = SharedSocket::Make(asio::ip::tcp::socket(ioContext), ioContext);

            const auto self = shared_from_this();
            mAcceptor.async_accept(
                    sharedSocket->socket(),
                    [self, callback, sharedSocket, this](std::error_code ec) mutable
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
        IoContextThreadPool::Ptr mIoContextThreadPool;
        asio::ip::tcp::acceptor mAcceptor;
    };

}// namespace bsio::net
