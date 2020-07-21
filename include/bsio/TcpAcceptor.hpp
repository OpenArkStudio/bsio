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
        TcpAcceptor() = default;

        TcpAcceptor(
            asio::io_context& listenContext,
            IoContextThreadPool::Ptr ioContextThreadPool,
            const asio::ip::tcp::endpoint& endpoint)
            :
            mIoContextThreadPool(std::move(ioContextThreadPool)),
            mAcceptor(std::make_shared<asio::ip::tcp::acceptor>(listenContext, endpoint))
        {
        }

        virtual ~TcpAcceptor()
        {
            close();
        }

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

            auto copySelf = *this;
            mAcceptor->async_accept(
                    sharedSocket->socket(),
                    [copySelf, callback, sharedSocket](std::error_code ec) mutable
                    {
                        if (!ec)
                        {
                            sharedSocket->context().post([=]()
                            {
                                callback(std::move(sharedSocket->socket()));
                            });
                        }
                        copySelf.doAccept(callback);
                    });
        }

    private:
        IoContextThreadPool::Ptr                    mIoContextThreadPool;
        std::shared_ptr<asio::ip::tcp::acceptor>    mAcceptor;
    };

} }