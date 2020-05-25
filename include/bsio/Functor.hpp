#pragma once

#include <functional>

#include <asio.hpp>

namespace bsio { namespace net {

    using SocketEstablishHandler = std::function<void(asio::ip::tcp::socket)>;
    using SocketFailedConnectHandler = std::function<void()>;
    using SocketProcessingHandler = std::function<void(asio::ip::tcp::socket&)>;

} }