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

        auto&   WithHttpSessionBuilder(common::HttpSessionBuilderCallback callback)
        {
            mHttpSessionBuilderCallback = std::move(callback);
            return *this;
        }

        HttpAcceptorBuilder& AddSocketProcessingHandler(SocketProcessingHandler handler) noexcept
        {
            mSocketOption.socketProcessingHandlers.push_back(std::move(handler));
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
                    mHttpSessionBuilderCallback);
        }

    private:
        TcpAcceptor::Ptr                    mAcceptor;
        internal::ServerSocketOption        mSocketOption;
        common::HttpSessionBuilderCallback  mHttpSessionBuilderCallback;
    };

} } }