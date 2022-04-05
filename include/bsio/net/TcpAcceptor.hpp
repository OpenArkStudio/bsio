#pragma once

#include <asio/basic_socket_acceptor.hpp>
#include <bsio/net/Functor.hpp>
#include <bsio/net/IoContextProvider.hpp>
#include <bsio/net/SharedSocket.hpp>
#include <functional>
#include <memory>
#include <utility>

namespace bsio::net {

class TcpAcceptor : private asio::noncopyable,
                    public std::enable_shared_from_this<TcpAcceptor>
{
public:
    using Ptr = std::shared_ptr<TcpAcceptor>;

    static Ptr Make(asio::io_context& listenContext,
                    const IoContextProvider::Ptr& ioContextProvider,
                    const asio::ip::tcp::endpoint& endpoint)
    {
        class make_shared_enabler : public TcpAcceptor
        {
        public:
            make_shared_enabler(asio::io_context& listenContext,
                                const IoContextProvider::Ptr& ioContextProvider,
                                const asio::ip::tcp::endpoint& endpoint)
                : TcpAcceptor(listenContext, ioContextProvider, endpoint)
            {
            }
        };

        auto acceptor = std::make_shared<make_shared_enabler>(listenContext, ioContextProvider, endpoint);
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
            IoContextProvider::Ptr ioContextProvider,
            const asio::ip::tcp::endpoint& endpoint)
        : mIoContextProvider(std::move(ioContextProvider)),
          mAcceptor(listenContext, endpoint)
    {
        mAcceptor.set_option(asio::socket_base::reuse_address(true));
    }

    void doAccept(const SocketEstablishHandler& callback)
    {
        if (!mAcceptor.is_open())
        {
            return;
        }

        auto& ioContext = mIoContextProvider->pickIoContext();
        auto sharedSocket = SharedSocket::Make(asio::ip::tcp::socket(ioContext), ioContext);
        mAcceptor.async_accept(
                sharedSocket->socket(),
                [self = shared_from_this(), this, callback, sharedSocket](std::error_code ec) mutable {
                    if (!ec)
                    {
                        sharedSocket->context().post([=]() {
                            callback(std::move(sharedSocket->socket()));
                        });
                    }
                    doAccept(callback);
                });
    }

private:
    IoContextProvider::Ptr mIoContextProvider;
    asio::ip::tcp::acceptor mAcceptor;
};

}// namespace bsio::net
