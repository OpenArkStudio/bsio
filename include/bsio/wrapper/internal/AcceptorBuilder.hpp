#pragma once

#include <bsio/Functor.hpp>
#include <bsio/TcpAcceptor.hpp>

namespace bsio {  namespace internal {

    struct ServerSocketOption final
    {
        SocketEstablishHandler  establishHandler;
        std::vector<SocketProcessingHandler> socketProcessingHandlers;
    };

    template<typename Derived>
    class BaseSocketAcceptorBuilder
    {
    public:
        virtual ~BaseSocketAcceptorBuilder() = default;

        Derived& WithAcceptor(TcpAcceptor::Ptr acceptor) noexcept
        {
            mAcceptor = acceptor;
            return static_cast<Derived&>(*this);
        }

        Derived& AddSocketProcessingHandler(SocketProcessingHandler handler) noexcept
        {
            mOption.socketProcessingHandlers.push_back(handler);
            return static_cast<Derived&>(*this);
        }

        void    start()
        {
            if (mAcceptor == nullptr)
            {
                throw std::runtime_error("acceptor is nullptr");
            }

            beforeStartAccept();

            if (mOption.establishHandler == nullptr)
            {
                throw std::runtime_error("establish handler is nullptr");
            }

            mAcceptor->startAccept([option = mOption](asio::ip::tcp::socket socket)
                {
                    for (const auto& handler : option.socketProcessingHandlers)
                    {
                        handler(socket);
                    }

                    option.establishHandler(std::move(socket));
                });

            endStartAccept();
        }

    private:
        virtual void beforeStartAccept()
        {}
        virtual void endStartAccept()
        {}

    protected:
        TcpAcceptor::Ptr    mAcceptor;
        ServerSocketOption  mOption;
    };

    template<typename Derived>
    class BaseServerSocketBuilderWithEstablishHandler : public BaseSocketAcceptorBuilder<Derived>
    {
    public:
        Derived& WithEstablishHandler(SocketEstablishHandler handler) noexcept
        {
            mOption.establishHandler = handler;
            return static_cast<Derived&>(*this);
        }
    };

} }