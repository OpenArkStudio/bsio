#pragma once

#include <bsio/net/Functor.hpp>
#include <bsio/net/IoContextProvider.hpp>
#include <bsio/net/SharedSocket.hpp>
#include <functional>
#include <memory>

namespace bsio::net {

class TcpConnector
{
public:
    using Ptr = std::shared_ptr<TcpConnector>;

    explicit TcpConnector(IoContextProvider::Ptr ioContextProvider)
        : mIoContextProvider(std::move(ioContextProvider))
    {
    }

    virtual ~TcpConnector() = default;

    void asyncConnect(
            asio::ip::tcp::endpoint endpoint,
            std::chrono::nanoseconds timeout,
            const SocketEstablishHandler& successCallback,
            const SocketFailedConnectHandler& failedCallback,
            const std::vector<SocketProcessingHandler>& socketProcessingHandlerList)
    {
        wrapperAsyncConnect(mIoContextProvider->pickIoContext(),
                            {std::move(endpoint)},
                            timeout,
                            successCallback,
                            failedCallback,
                            socketProcessingHandlerList);
    }

    static void asyncConnect(
            asio::io_context& ioContext,
            asio::ip::tcp::endpoint endpoint,
            std::chrono::nanoseconds timeout,
            const SocketEstablishHandler& successCallback,
            const SocketFailedConnectHandler& failedCallback,
            const std::vector<SocketProcessingHandler>& socketProcessingHandlerList)
    {
        wrapperAsyncConnect(ioContext,
                            {std::move(endpoint)},
                            timeout,
                            successCallback,
                            failedCallback,
                            socketProcessingHandlerList);
    }

private:
    static void wrapperAsyncConnect(
            asio::io_context& ioContext,
            const std::vector<asio::ip::tcp::endpoint>& endpoints,
            std::chrono::nanoseconds timeout,
            const SocketEstablishHandler& successCallback,
            const SocketFailedConnectHandler& failedCallback,
            const std::vector<SocketProcessingHandler>& socketProcessingHandlerList)
    {
        auto sharedSocket = SharedSocket::Make(
                asio::ip::tcp::socket(ioContext),
                ioContext);
        auto timeoutTimer = WrapperIoContext::RunAfter(ioContext, timeout, [=]() {
            failedCallback();
        });

        asio::async_connect(sharedSocket->socket(),
                            endpoints,
                            [=](std::error_code ec, const asio::ip::tcp::endpoint&) {
                                timeoutTimer->cancel();
                                if (ec)
                                {
                                    if (failedCallback != nullptr)
                                    {
                                        failedCallback();
                                    }
                                    return;
                                }

                                for (const auto& handler : socketProcessingHandlerList)
                                {
                                    handler(sharedSocket->socket());
                                }
                                if (successCallback != nullptr)
                                {
                                    successCallback(std::move(sharedSocket->socket()));
                                }
                            });
    }

private:
    IoContextProvider::Ptr mIoContextProvider;
};

}// namespace bsio::net
