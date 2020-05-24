#pragma once

#include <utility>
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

        Derived& WithClosedHandler(TcpSession::ClosedHandler handler)
        {
            mOption->closedHandler = std::move(handler);
            return static_cast<Derived&>(*this);
        }

    protected:
        std::shared_ptr<TcpSessionOption> mOption;
    };

    template<typename Derived>
    class BaseSessionBuilderWithEnter : public BaseSessionBuilder<Derived>
    {
    public:
        Derived& AddEnterCallback(const TcpSessionEstablishHandler& handler)
        {
            mOption->establishHandler.push_back(handler);
            return static_cast<Derived&>(*this);
        }
    };

    template<typename Derived>
    class SessionBuilderWithDataHandler : public BaseSessionBuilderWithEnter< Derived>
    {
    public:
        Derived& WithDataHandler(const TcpSession::DataHandler& handler)
        {
            BaseSessionBuilderWithEnter< Derived>::mOption->dataHandler = handler;
            return static_cast<Derived&>(*this);
        }
    };

} }
