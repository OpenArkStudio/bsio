#pragma once

#include <bsio/TcpAcceptor.hpp>
#include <bsio/http/HttpService.hpp>
#include <bsio/wrapper/internal/Option.hpp>
#include <bsio/wrapper/internal/Common.hpp>

namespace bsio { namespace net { namespace wrapper {

    class HttpAcceptorBuilder
    {
    public:
        HttpAcceptorBuilder& WithAcceptor(TcpAcceptor::Ptr acceptor) noexcept
        {
            mAcceptor = std::move(acceptor);
            return *this;
        }

        HttpAcceptorBuilder& AddSocketProcessingHandler(SocketProcessingHandler handler) noexcept
        {
            mSocketOption.socketProcessingHandlers.push_back(std::move(handler));
            return *this;
        }

        HttpAcceptorBuilder& WithEnterCallback(http::HttpSession::EnterCallback callback) noexcept
        {
            mEnterCallback = std::move(callback);
            return *this;
        }

        HttpAcceptorBuilder& WithParserCallback(http::HttpSession::HttpParserCallback callback) noexcept
        {
            mParserCallback = std::move(callback);
            return *this;
        }

        HttpAcceptorBuilder& WithWsCallback(http::HttpSession::WsCallback handler) noexcept
        {
            mWsCallback = std::move(handler);
            return *this;
        }

        HttpAcceptorBuilder& WithRecvBufferSize(size_t size) noexcept
        {
            mTcpSessionOption.recvBufferSize = size;
            return *this;
        }

        void    start()
        {
            if (mAcceptor == nullptr)
            {
                throw std::runtime_error("acceptor is nullptr");
            }

            setupHttp();

            mAcceptor->startAccept([option = mSocketOption](asio::ip::tcp::socket socket)
                {
                    for (const auto& handler : option.socketProcessingHandlers)
                    {
                        handler(socket);
                    }
                    option.establishHandler(std::move(socket));
                });
        }

    private:
        void setupHttp()
        {
            mSocketOption.establishHandler = common::generateHttpEstablishHandler(
                    mTcpSessionOption,
                    mEnterCallback,
                    mParserCallback,
                    mWsCallback);
        }

    private:
        TcpAcceptor::Ptr                mAcceptor;
        internal::ServerSocketOption    mSocketOption;
        internal::TcpSessionOption      mTcpSessionOption;

        http::HttpSession::EnterCallback mEnterCallback;
        http::HttpSession::HttpParserCallback mParserCallback;
        http::HttpSession::WsCallback    mWsCallback;
    };

} } }