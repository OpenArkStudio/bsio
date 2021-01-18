#pragma once

#include <asio.hpp>
#include <functional>

namespace bsio::net {

using SocketEstablishHandler = std::function<void(asio::ip::tcp::socket)>;
using SocketFailedConnectHandler = std::function<void()>;
using SocketProcessingHandler = std::function<void(asio::ip::tcp::socket&)>;

}// namespace bsio::net
