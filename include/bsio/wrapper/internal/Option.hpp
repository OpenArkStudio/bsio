#pragma once

#include <bsio/Functor.hpp>
#include <bsio/TcpSession.hpp>

namespace bsio::net ::wrapper ::internal
{

    struct ServerSocketOption final
    {
        SocketEstablishHandler establishHandler;
        std::vector<SocketProcessingHandler> socketProcessingHandlers;
    };

    struct SocketConnectOption final
    {
        asio::ip::tcp::endpoint endpoint;
        std::chrono::nanoseconds timeout = std::chrono::seconds(10);
        SocketEstablishHandler establishHandler;
        SocketFailedConnectHandler failedHandler;
        std::vector<SocketProcessingHandler> socketProcessingHandlers;
    };

    struct TcpSessionOption final
    {
        size_t recvBufferSize = 0;
        std::vector<TcpSessionEstablishHandler> establishHandlers;
        TcpSession::DataHandler dataHandler;
        TcpSession::ClosedHandler closedHandler;
    };

}// namespace bsio::net::wrapper::internal
