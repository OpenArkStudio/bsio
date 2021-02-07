#pragma once

#include <bsio/net/TcpAcceptor.hpp>
#include <bsio/net/http/HttpService.hpp>
#include <bsio/net/wrapper/internal/HttpSessionBuilder.hpp>
#include <bsio/net/wrapper/internal/Option.hpp>

namespace bsio::net::wrapper {

class HttpSessionBuilder : public internal::BaseHttpSessionBuilder<HttpSessionBuilder>,
                           public asio::noncopyable
{
};

class HttpAcceptorBuilder
{
public:
    using HttpSessionBuilderCallback = std::function<void(HttpSessionBuilder&)>;

    HttpAcceptorBuilder& WithAcceptor(TcpAcceptor::Ptr acceptor) noexcept
    {
        mAcceptor = std::move(acceptor);
        return *this;
    }

    auto& WithHttpSessionBuilder(HttpSessionBuilderCallback callback)
    {
        mHttpSessionBuilderCallback = std::move(callback);
        return *this;
    }

    HttpAcceptorBuilder& AddSocketProcessingHandler(SocketProcessingHandler handler) noexcept
    {
        mSocketProcessingHandlers.emplace_back(std::move(handler));
        return *this;
    }

    void start()
    {
        if (mAcceptor == nullptr)
        {
            throw std::runtime_error("acceptor is nullptr");
        }
        if (mHttpSessionBuilderCallback == nullptr)
        {
            throw std::runtime_error("session builder is nullptr");
        }

        auto establishHandler = [callback = mHttpSessionBuilderCallback](asio::ip::tcp::socket socket) {
            HttpSessionBuilder builder;
            callback(builder);
            const auto& option = builder.SessionOption();
            const auto session = TcpSession::Make(std::move(socket),
                                                  option.recvBufferSize,
                                                  nullptr,
                                                  option.closedHandler);
            internal::setupHttpSession(session,
                                       builder.EnterCallback(),
                                       builder.ParserCallback(),
                                       builder.WsCallback());
        };
        mAcceptor->startAccept([callbacks = mSocketProcessingHandlers,
                                establishHandler = std::move(establishHandler)](asio::ip::tcp::socket socket) {
            for (const auto& handler : callbacks)
            {
                handler(socket);
            }
            establishHandler(std::move(socket));
        });
    }

private:
    TcpAcceptor::Ptr mAcceptor;
    std::vector<SocketProcessingHandler> mSocketProcessingHandlers;
    HttpSessionBuilderCallback mHttpSessionBuilderCallback;
};

}// namespace bsio::net::wrapper
