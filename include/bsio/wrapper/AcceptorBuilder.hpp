#pragma once

#include <bsio/TcpAcceptor.hpp>
#include <bsio/wrapper/internal/Option.hpp>
#include <bsio/wrapper/TcpSessionBuilder.hpp>

namespace bsio { namespace net { namespace wrapper {

    class TcpSessionAcceptorBuilder
    {
    public:
        virtual ~TcpSessionAcceptorBuilder() = default;

        using SessionOptionBuilderCallback = std::function<void(SessionOptionBuilder&)>;

        TcpSessionAcceptorBuilder& WithAcceptor(TcpAcceptor::Ptr acceptor) noexcept
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
            if (mAcceptor == nullptr)
            {
                throw std::runtime_error("acceptor is nullptr");
            }
            if (mSessionOptionBuilderCallback == nullptr)
            {
                throw std::runtime_error("fuck callback is nullptr");
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

            mAcceptor->startAccept([option = mServerSocketOption](asio::ip::tcp::socket socket)
                {
                    for (const auto& handler : option.socketProcessingHandlers)
                    {
                        handler(socket);
                    }
                    option.establishHandler(std::move(socket));
                });
        }

    private:
        TcpAcceptor::Ptr                mAcceptor;
        internal::ServerSocketOption    mServerSocketOption;
        SessionOptionBuilderCallback    mSessionOptionBuilderCallback;
    };

} } }