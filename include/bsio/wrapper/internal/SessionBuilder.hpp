#pragma once

#include <bsio/TcpSession.hpp>

namespace bsio {

    using TcpSessionEstablishHandler = std::function<void(TcpSession::Ptr)>;
}

namespace bsio { namespace internal {

    struct TcpSessionOption final
    {
        size_t  recvBufferSize = 0;
        std::vector< TcpSessionEstablishHandler> establishHandler;
        TcpSession::DataHandler dataHandler;
        TcpSession::ClosedHandler closedHandler;
    };

    template<typename Derived>
    class BaseSessionBuilder
    {
    public:
        BaseSessionBuilder()
            :
            mOption(std::make_shared<TcpSessionOption>())
        {
        }

        virtual ~BaseSessionBuilder() = default;

        Derived& WithRecvBufferSize(size_t size)
        {
            mOption->recvBufferSize = size;
            return static_cast<Derived&>(*this);
        }

        Derived& AddEnterCallback(TcpSessionEstablishHandler handler)
        {
            mOption->establishHandler.push_back(handler);
            return static_cast<Derived&>(*this);
        }

        Derived& WithClosedHandler(TcpSession::ClosedHandler handler)
        {
            mOption->closedHandler = handler;
            return static_cast<Derived&>(*this);
        }

    protected:
        std::shared_ptr<TcpSessionOption> mOption;
    };

    template<typename Derived>
    class SessionBuilderWithDataHandler : public BaseSessionBuilder< Derived>
    {
    public:
        Derived& WithDataHandler(TcpSession::DataHandler handler)
        {
            BaseSessionBuilder< Derived>::mOption->dataHandler = handler;
            return static_cast<Derived&>(*this);
        }
    };

} }
