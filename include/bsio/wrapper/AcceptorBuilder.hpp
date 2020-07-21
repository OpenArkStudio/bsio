#pragma once

#include <optional>
#include <bsio/TcpAcceptor.hpp>
#include <bsio/wrapper/internal/Option.hpp>
#include <bsio/wrapper/internal/TcpSessionBuilder.hpp>

namespace bsio { namespace net { namespace wrapper {

    using SessionOptionBuilder = internal::SessionOptionBuilder;
    class TcpSessionAcceptorBuilder
    {
    public:
        virtual ~TcpSessionAcceptorBuilder() = default;

        using SessionOptionBuilderCallback = std::function<void(SessionOptionBuilder&)>;

        TcpSessionAcceptorBuilder& WithAcceptor(TcpAcceptor acceptor) noexcept
        {
            mAcceptor = std::move(acceptor);
            return *this;
        }

        TcpSessionAcceptorBuilder& WithSessionOptionBuilder(SessionOptionBuilderCallback callback)
        {
            mSessionOptionBuilderCallback = std::move(callback);
            return *this;
        }

        TcpSessionAcceptorBuilder& AddSocketProcessingHandler(SocketProcessingHandler handler) noexcept
        {
            mServerSocketOption.socketProcessingHandlers.push_back(std::move(handler));
            return *this;
        }

        void    start()
        {
            if (!mAcceptor)
            {
                throw std::runtime_error("acceptor is empty");
            }
            if (mSessionOptionBuilderCallback == nullptr)
            {
                throw std::runtime_error("session builder is nullptr");
            }

            // setting establishHandlers
            mServerSocketOption.establishHandler =
                [builderCallback = mSessionOptionBuilderCallback](asio::ip::tcp::socket socket)
            {
                SessionOptionBuilder option;
                builderCallback(option);

                if (option.Option().dataHandler == nullptr)
                {
                    throw std::runtime_error("data handler not setting");
                }

                const auto session = TcpSession::Make(std::move(socket),
                                                      option.Option().recvBufferSize,
                                                      option.Option().dataHandler,
                                                      option.Option().closedHandler);
                for (const auto& callback : option.Option().establishHandlers)
                {
                    callback(session);
                }
            };

            mAcceptor.startAccept([option = mServerSocketOption](asio::ip::tcp::socket socket)
                {
                    for (const auto& handler : option.socketProcessingHandlers)
                    {
                        handler(socket);
                    }
                    option.establishHandler(std::move(socket));
                });
        }

    private:
        std::optional<TcpAcceptor>      mAcceptor;
        internal::ServerSocketOption    mServerSocketOption;
        SessionOptionBuilderCallback    mSessionOptionBuilderCallback;
    };

} } }