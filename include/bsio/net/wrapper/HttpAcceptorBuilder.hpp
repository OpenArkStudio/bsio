#pragma once

#include <bsio/net/TcpAcceptor.hpp>
#include <bsio/net/http/HttpService.hpp>
#include <bsio/net/wrapper/AcceptorBuilder.hpp>
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
        mSessionAcceptorBuilder.WithAcceptor(acceptor);
        return *this;
    }

    HttpAcceptorBuilder& WithRecvBufferSize(size_t size) noexcept
    {
        mSessionAcceptorBuilder.WithRecvBufferSize(size);
        return *this;
    }

    HttpAcceptorBuilder& AddSocketProcessingHandler(SocketProcessingHandler handler) noexcept
    {
        mSessionAcceptorBuilder.AddSocketProcessingHandler(handler);
        return *this;
    }

    auto& WithHttpSessionBuilder(HttpSessionBuilderCallback callback)
    {
        mHttpSessionBuilderCallback = std::move(callback);
        return *this;
    }

    void start()
    {
        if (mHttpSessionBuilderCallback == nullptr)
        {
            throw std::runtime_error("session builder is nullptr");
        }

        mSessionAcceptorBuilder.WithSessionOptionBuilder([callback = mHttpSessionBuilderCallback](SessionOptionBuilder& sessionBuilder) {
            sessionBuilder.AddEnterCallback([callback = callback](TcpSession::Ptr session) {
                HttpSessionBuilder httpBuilder;
                callback(httpBuilder);
                internal::setupHttpSession(std::move(session), httpBuilder.EnterCallback(),
                                           httpBuilder.ParserCallback(),
                                           httpBuilder.WsCallback(),
                                           httpBuilder.ClosedCallback());
            });
        });

        return mSessionAcceptorBuilder.start();
    }

private:
    TcpSessionAcceptorBuilder mSessionAcceptorBuilder;
    HttpSessionBuilderCallback mHttpSessionBuilderCallback;
};

}// namespace bsio::net::wrapper
