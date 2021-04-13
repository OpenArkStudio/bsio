#pragma once

#include <bsio/net/wrapper/ConnectorBuilder.hpp>
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
        mSessionConnectorBuilder.WithConnector(connector);
        return *this;
    }

    HttpConnectorBuilder& WithEndpoint(asio::ip::tcp::endpoint endpoint) noexcept
    {
        mSessionConnectorBuilder.WithEndpoint(endpoint);
        return *this;
    }

    HttpConnectorBuilder& WithTimeout(std::chrono::nanoseconds timeout) noexcept
    {
        mSessionConnectorBuilder.WithTimeout(timeout);
        return *this;
    }

    HttpConnectorBuilder& WithFailedHandler(SocketFailedConnectHandler handler) noexcept
    {
        mSessionConnectorBuilder.WithFailedHandler(handler);
        return *this;
    }

    HttpConnectorBuilder& WithRecvBufferSize(size_t size) noexcept
    {
        mSessionConnectorBuilder.WithRecvBufferSize(size);
        return *this;
    }

    HttpConnectorBuilder& AddSocketProcessingHandler(SocketProcessingHandler handler) noexcept
    {
        mSessionConnectorBuilder.AddSocketProcessingHandler(handler);
        return *this;
    }

    void asyncConnect()
    {
        auto httpSession = std::make_shared<http::HttpSession>(nullptr, ParserCallback(), WsCallback(), nullptr, ClosedCallback());
        auto [dataHandler, eofHandler, closedHandler] = internal::makeHttpHandlers(httpSession);

        mSessionConnectorBuilder.WithDataHandler(dataHandler);
        mSessionConnectorBuilder.WithEofHandler(eofHandler);
        mSessionConnectorBuilder.WithClosedHandler(closedHandler);

        mSessionConnectorBuilder.AddEstablishHandler([enterCallback = EnterCallback(), httpSession](TcpSession::Ptr session) {
            httpSession->setSession(session);
            if (enterCallback != nullptr)
            {
                enterCallback(httpSession);
            }
        });

        return mSessionConnectorBuilder.asyncConnect();
    }

private:
    TcpSessionConnectorBuilder mSessionConnectorBuilder;
};

}// namespace bsio::net::wrapper
