#pragma once

#include <bsio/net/wrapper/internal/HttpSessionBuilder.hpp>
#include <bsio/net/wrapper/internal/Option.hpp>
#include <optional>

namespace bsio::net::wrapper {

class HttpConnectorBuilder : public internal::BaseHttpSessionBuilder<HttpConnectorBuilder>
{
public:
    virtual ~HttpConnectorBuilder() = default;

    HttpConnectorBuilder& WithConnector(TcpConnector connector) noexcept
    {
        mConnector = std::move(connector);
        return *this;
    }

    HttpConnectorBuilder& WithEndpoint(asio::ip::tcp::endpoint endpoint) noexcept
    {
        mSocketOption.endpoint = std::move(endpoint);
        return *this;
    }

    HttpConnectorBuilder& WithTimeout(std::chrono::nanoseconds timeout) noexcept
    {
        mSocketOption.timeout = timeout;
        return *this;
    }

    HttpConnectorBuilder& WithFailedHandler(SocketFailedConnectHandler handler) noexcept
    {
        mSocketOption.failedHandler = std::move(handler);
        return *this;
    }

    HttpConnectorBuilder& WithRecvBufferSize(size_t size) noexcept
    {
        mReceiveBufferSize = size;
        return *this;
    }

    HttpConnectorBuilder& AddSocketProcessingHandler(SocketProcessingHandler handler) noexcept
    {
        mSocketOption.socketProcessingHandlers.emplace_back(std::move(handler));
        return *this;
    }

    void asyncConnect()
    {
        if (!mConnector)
        {
            throw std::runtime_error("connector is empty");
        }

        mConnector->asyncConnect(
                mSocketOption.endpoint,
                mSocketOption.timeout,
                [*this](asio::ip::tcp::socket socket) {
                    const auto& option = SessionOption();
                    const auto session = TcpSession::Make(std::move(socket),
                                                          mReceiveBufferSize,
                                                          nullptr,
                                                          option.closedHandler);
                    internal::setupHttpSession(session,
                                               EnterCallback(),
                                               ParserCallback(),
                                               WsCallback());
                },
                mSocketOption.failedHandler,
                mSocketOption.socketProcessingHandlers);
    }

private:
    std::optional<TcpConnector> mConnector;
    internal::SocketConnectOption mSocketOption;
    size_t mReceiveBufferSize = {0};
};

}// namespace bsio::net::wrapper
