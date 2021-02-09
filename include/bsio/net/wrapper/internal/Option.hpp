#pragma once

#include <bsio/net/Functor.hpp>
#include <bsio/net/TcpSession.hpp>

namespace bsio::net::wrapper::internal {

struct SocketConnectOption final {
    asio::ip::tcp::endpoint endpoint;
    std::chrono::nanoseconds timeout = std::chrono::seconds(10);
    SocketFailedConnectHandler failedHandler;
    std::vector<SocketProcessingHandler> socketProcessingHandlers;
};

struct TcpSessionOption final {
    std::vector<TcpSessionEstablishHandler> establishHandlers;
    TcpSession::DataHandler dataHandler;
    TcpSession::ClosedHandler closedHandler;
};

}// namespace bsio::net::wrapper::internal
