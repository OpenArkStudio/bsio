#pragma once

#include <bsio/TcpAcceptor.hpp>
#include <bsio/wrapper/internal/Option.hpp>

namespace bsio { namespace net { namespace wrapper {

    class SessionOptionBuilder
    {
    public:
        auto& WithRecvBufferSize(size_t size) noexcept
        {
            mTcpSessionOption.recvBufferSize = size;
            return *this;
        }

        auto& AddEnterCallback(TcpSessionEstablishHandler handler) noexcept
        {
            mTcpSessionOption.establishHandlers.push_back(std::move(handler));
            return *this;
        }

        auto& WithClosedHandler(TcpSession::ClosedHandler handler) noexcept
        {
            mTcpSessionOption.closedHandler = std::move(handler);
            return *this;
        }

        auto& WithDataHandler(TcpSession::DataHandler handler) noexcept
        {
            mTcpSessionOption.dataHandler = std::move(handler);
            return *this;
        }

        const internal::TcpSessionOption&   Option() const
        {
            return mTcpSessionOption;
        }

    private:
        internal::TcpSessionOption  mTcpSessionOption;
    };

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

        TcpSessionAcceptorBuilder& WithSessionOptionBuilderCallback(SessionOptionBuilderCallback callback)
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
        TcpAcceptor::Ptr    mAcceptor;
        internal::ServerSocketOption    mServerSocketOption;
        SessionOptionBuilderCallback    mSessionOptionBuilderCallback;
    };

} } }