#pragma once

#include <bsio/TcpAcceptor.hpp>
#include <bsio/http/HttpService.hpp>
#include <bsio/wrapper/internal/Option.hpp>
#include <bsio/wrapper/internal/HttpSessionBuilder.hpp>

namespace bsio { namespace net { namespace wrapper {

    class HttpSessionBuilder :  public internal::BaseHttpSessionBuilder<HttpSessionBuilder>,
                                public asio::noncopyable
    {};

    class HttpAcceptorBuilder
    {
    public:
        using HttpSessionBuilderCallback = std::function<void(HttpSessionBuilder&)>;

        HttpAcceptorBuilder& WithAcceptor(TcpAcceptor::Ptr acceptor) noexcept
        {
            mAcceptor = std::move(acceptor);
            return *this;
        }

        auto&   WithHttpSessionBuilder(HttpSessionBuilderCallback callback)
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
            if (mHttpSessionBuilderCallback == nullptr)
            {
                throw std::runtime_error("session builder is nullptr");
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
            mSocketOption.establishHandler = [callback = mHttpSessionBuilderCallback](asio::ip::tcp::socket socket)
            {
                HttpSessionBuilder builder;
                callback(builder);
                internal::setupHttpSession(std::move(socket),
                                           builder.SessionOption(),
                                           builder.EnterCallback(),
                                           builder.ParserCallback(),
                                           builder.WsCallback());
            };
        }

    private:
        TcpAcceptor::Ptr                    mAcceptor;
        internal::ServerSocketOption        mSocketOption;
        HttpSessionBuilderCallback          mHttpSessionBuilderCallback;
    };

} } }