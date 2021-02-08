#pragma once

#include <bsio/net/TcpAcceptor.hpp>
#include <bsio/net/wrapper/internal/Option.hpp>
#include <bsio/net/wrapper/internal/TcpSessionBuilder.hpp>

namespace bsio::net::wrapper {

using SessionOptionBuilder = internal::SessionOptionBuilder;

template<typename Derived>
class BaseTcpSessionAcceptorBuilder
{
public:
    virtual ~BaseTcpSessionAcceptorBuilder() = default;

    using SessionOptionBuilderCallback = std::function<void(SessionOptionBuilder &)>;

    Derived &WithAcceptor(TcpAcceptor::Ptr acceptor) noexcept
    {
        mAcceptor = std::move(acceptor);
        return static_cast<Derived &>(*this);
    }

    Derived &AddSocketProcessingHandler(SocketProcessingHandler handler) noexcept
    {
        mSocketProcessingHandlers.emplace_back(std::move(handler));
        return static_cast<Derived &>(*this);
    }

    Derived &WithSessionOptionBuilder(SessionOptionBuilderCallback callback)
    {
        mSessionOptionBuilderCallback = std::move(callback);
        return static_cast<Derived &>(*this);
    }

    Derived &WithRecvBufferSize(size_t size) noexcept
    {
        mReceiveBufferSize = size;
        return static_cast<Derived &>(*this);
    }

    void start()
    {
        if (mAcceptor == nullptr)
        {
            throw std::runtime_error("acceptor is nullptr");
        }
        if (mSessionOptionBuilderCallback == nullptr)
        {
            throw std::runtime_error("session builder is nullptr");
        }

        auto establishHandler = [builderCallback = mSessionOptionBuilderCallback,
                                 receiveBufferSize = mReceiveBufferSize](asio::ip::tcp::socket socket) {
            SessionOptionBuilder option;
            builderCallback(option);

            const auto session = TcpSession::Make(std::move(socket),
                                                  receiveBufferSize,
                                                  option.Option().dataHandler,
                                                  option.Option().closedHandler);
            for (const auto &callback : option.Option().establishHandlers)
            {
                callback(session);
            }
        };

        mAcceptor->startAccept([handlers = mSocketProcessingHandlers,
                                establishHandler = std::move(establishHandler)](asio::ip::tcp::socket socket) {
            for (const auto &handler : handlers)
            {
                handler(socket);
            }
            establishHandler(std::move(socket));
        });
    }

private:
    TcpAcceptor::Ptr mAcceptor;
    std::vector<SocketProcessingHandler> mSocketProcessingHandlers;
    SessionOptionBuilderCallback mSessionOptionBuilderCallback;
    size_t mReceiveBufferSize = {0};
};

class TcpSessionAcceptorBuilder : public BaseTcpSessionAcceptorBuilder<TcpSessionAcceptorBuilder>
{
};

}// namespace bsio::net::wrapper
