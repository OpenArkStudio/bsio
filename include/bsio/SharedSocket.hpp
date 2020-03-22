#pragma once

#include <memory>

#include <asio.hpp>

namespace bsio {

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

        asio::io_context& context()
        {
            return mIoContext;
        }

        SharedSocket(asio::ip::tcp::socket socket, asio::io_context& ioContext)
            :
            mSocket(std::move(socket)),
            mIoContext(ioContext)
        {
        }

    private:
        static SharedSocket::Ptr Make(asio::ip::tcp::socket socket, 
            asio::io_context& ioContext)
        {
            return std::make_shared<SharedSocket>(std::move(socket), ioContext);
        }

    private:
        asio::ip::tcp::socket   mSocket;
        asio::io_context&       mIoContext;

        friend class TcpAcceptor;
        friend class TcpConnector;
    };

}