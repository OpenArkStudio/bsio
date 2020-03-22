#pragma once

#include <memory>
#include <functional>

#include <bsio/IoContextThreadPool.hpp>
#include <bsio/Functor.hpp>

namespace bsio {

    class TcpConnector
    {
    public:
        using Ptr = std::shared_ptr<TcpConnector>;

        explicit TcpConnector(IoContextThreadPool::Ptr ioContextThreadPool)
            :
            mIoContextThreadPool(ioContextThreadPool)
        {
        }

        void    asyncConnect(
            asio::ip::tcp::endpoint endpoint,
            std::chrono::nanoseconds timeout,
            SocketEstablishHandler successCallback,
            SocketFailedConnectHandler failedCallback)
        {
            wrapperAsyncConnect(mIoContextThreadPool->pickIoContextThread(), 
                { endpoint }, 
                timeout, 
                successCallback, 
                failedCallback);
        }

        void    asyncConnect(
            std::shared_ptr<IoContextThread> ioContextThread,
            asio::ip::tcp::endpoint endpoint,
            std::chrono::nanoseconds timeout,
            SocketEstablishHandler successCallback,
            SocketFailedConnectHandler failedCallback)
        {
            wrapperAsyncConnect(ioContextThread, 
                { endpoint }, 
                timeout, 
                successCallback, 
                failedCallback);
        }

    private:
        void    wrapperAsyncConnect(
            IoContextThread::Ptr ioContextThread,
            std::vector<asio::ip::tcp::endpoint> endpoints,
            std::chrono::nanoseconds timeout,
            SocketEstablishHandler successCallback,
            SocketFailedConnectHandler failedCallback)
        {
            auto sharedSocket = SharedSocket::Make(
                asio::ip::tcp::socket(ioContextThread->context()), 
                ioContextThread->context());
            auto timeoutTimer = ioContextThread->wrapperIoContext().runAfter(timeout, [=]() {
                    failedCallback();
                });

            asio::async_connect(sharedSocket->socket(),
                endpoints,
                [=](std::error_code ec, asio::ip::tcp::endpoint) {
                    timeoutTimer->cancel();
                    if (!ec)
                    {
                        successCallback(std::move(sharedSocket->socket()));
                    }
                    else
                    {
                        failedCallback();
                    }
                });
        }

    private:
        IoContextThreadPool::Ptr mIoContextThreadPool;
    };

}