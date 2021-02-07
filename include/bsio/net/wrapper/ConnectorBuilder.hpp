#pragma once

#include <bsio/net/TcpSession.hpp>
#include <bsio/net/wrapper/internal/Option.hpp>
#include <bsio/net/wrapper/internal/TcpSessionBuilder.hpp>
#include <functional>
#include <memory>
#include <optional>

namespace bsio::net::wrapper {

template<typename Derived>
class BaseTcpSessionConnectorBuilder : public internal::BaseSessionOptionBuilder<Derived>
{
public:
    virtual ~BaseTcpSessionConnectorBuilder() = default;

    Derived& WithConnector(TcpConnector connector) noexcept
    {
        mConnector = std::move(connector);
        return static_cast<Derived&>(*this);
    }

    Derived& WithEndpoint(asio::ip::tcp::endpoint endpoint) noexcept
    {
        mSocketOption.endpoint = std::move(endpoint);
        return static_cast<Derived&>(*this);
    }

    Derived& WithTimeout(std::chrono::nanoseconds timeout) noexcept
    {
        mSocketOption.timeout = timeout;
        return static_cast<Derived&>(*this);
    }

    Derived& WithFailedHandler(SocketFailedConnectHandler handler) noexcept
    {
        mSocketOption.failedHandler = std::move(handler);
        return static_cast<Derived&>(*this);
    }

    Derived& AddSocketProcessingHandler(SocketProcessingHandler handler) noexcept
    {
        mSocketOption.socketProcessingHandlers.emplace_back(std::move(handler));
        return static_cast<Derived&>(*this);
    }

    void asyncConnect()
    {
        using Base = internal::BaseSessionOptionBuilder<Derived>;

        if (!mConnector)
        {
            throw std::runtime_error("connector is empty");
        }

        mConnector->asyncConnect(
                mSocketOption.endpoint,
                mSocketOption.timeout,
                [option = Base::Option()](asio::ip::tcp::socket socket) {
                    const auto session = TcpSession::Make(
                            std::move(socket),
                            option.recvBufferSize,
                            option.dataHandler,
                            option.closedHandler);
                    for (const auto& callback : option.establishHandlers)
                    {
                        callback(session);
                    }
                },
                mSocketOption.failedHandler,
                mSocketOption.socketProcessingHandlers);
    }

private:
    std::optional<TcpConnector> mConnector;
    internal::SocketConnectOption mSocketOption;
};

class TcpSessionConnectorBuilder : public BaseTcpSessionConnectorBuilder<TcpSessionConnectorBuilder>
{
};

}// namespace bsio::net::wrapper
