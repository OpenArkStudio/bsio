#pragma once

#include <asio.hpp>
#include <memory>

namespace bsio::net
{
    class TcpAcceptor;
    class TcpConnector;

    class SharedSocket : public asio::noncopyable
    {
    public:
        using Ptr = std::shared_ptr<SharedSocket>;

        virtual ~SharedSocket() = default;

        asio::ip::tcp::socket& socket()
        {
            return mSocket;
        }

        asio::io_context& context() const
        {
            return mIoContext;
        }

        SharedSocket(asio::ip::tcp::socket socket, asio::io_context& ioContext)
            : mSocket(std::move(socket)),
              mIoContext(ioContext)
        {
        }

    private:
        static Ptr Make(asio::ip::tcp::socket socket,
                        asio::io_context& ioContext)
        {
            return std::make_shared<SharedSocket>(std::move(socket), ioContext);
        }

    private:
        asio::ip::tcp::socket mSocket;
        asio::io_context& mIoContext;

        friend class TcpAcceptor;
        friend class TcpConnector;
    };

}// namespace bsio::net
