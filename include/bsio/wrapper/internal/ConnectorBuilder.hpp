#pragma once

#include <memory>
#include <functional>
#include <utility>

#include <bsio/TcpConnector.hpp>
#include <bsio/Functor.hpp>
#include <bsio/TcpSession.hpp>
#include <bsio/wrapper/internal/SessionBuilder.hpp>

namespace bsio { namespace internal {
        
    struct SocketConnectOption final
    {
        asio::ip::tcp::endpoint endpoint;
        std::chrono::nanoseconds timeout = std::chrono::seconds(10);
        SocketEstablishHandler  establishHandler;
        SocketFailedConnectHandler  failedHandler;
        std::vector< SocketProcessingHandler> socketProcessingHandlers;
    };

    template<typename Derived>
    class BaseSocketConnectBuilder
    {
    public:
        virtual ~BaseSocketConnectBuilder() = default;

        Derived& WithConnector(TcpConnector* connector)
        {
            mConnector = connector;
            return static_cast<Derived&>(*this);
        }

        Derived& WithEndpoint(asio::ip::tcp::endpoint endpoint)
        {
            mOption.endpoint = std::move(endpoint);
            return static_cast<Derived&>(*this);
        }

        Derived& WithTimeout(std::chrono::nanoseconds timeout)
        {
            mOption.timeout = timeout;
            return static_cast<Derived&>(*this);
        }

        Derived& WithFailedHandler(SocketFailedConnectHandler handler)
        {
            mOption.failedHandler = std::move(handler);
            return static_cast<Derived&>(*this);
        }

        Derived& AddSocketProcessingHandler(const SocketProcessingHandler& handler)
        {
            mOption.socketProcessingHandlers.push_back(handler);
            return static_cast<Derived&>(*this);
        }

        void asyncConnect()
        {
            beforeAsyncConnect();

            mConnector->asyncConnect(
                mOption.endpoint,
                mOption.timeout,
                mOption.establishHandler,
                mOption.failedHandler,
                mOption.socketProcessingHandlers);

            endAsyncConnect();
        }

    protected:
        virtual void beforeAsyncConnect()
        {
            if (mConnector == nullptr)
            {
                throw std::runtime_error("connector is nullptr");
            }
        }

        virtual void endAsyncConnect()
        {
        }

    protected:
        TcpConnector* mConnector = nullptr;
        SocketConnectOption mOption;
    };

    template<typename Derived>
    class SocketConnectBuilderWithEstablishHandler : public BaseSocketConnectBuilder<Derived>
    {
    public:
        Derived& WithEstablishHandler(const SocketEstablishHandler& handler)
        {
            if (mHasSettingEstablishHandler)
            {
                throw std::runtime_error("already setting establish handler");
            }
            mHasSettingEstablishHandler = true;
            BaseSocketConnectBuilder<Derived>::mOption.establishHandler = handler;

            return static_cast<Derived&>(*this);
        }

    private:
        void endAsyncConnect() override
        {
            mHasSettingEstablishHandler = false;
            BaseSocketConnectBuilder<Derived>::endAsyncConnect();
        }

    private:
        bool    mHasSettingEstablishHandler = false;
    };

    template<typename Derived, template<typename T> class SessionBuilder>
    class BaseTcpSessionConnectBuilder :    public internal::BaseSocketConnectBuilder<Derived>,
                                            public SessionBuilder<Derived>
    {
    private:
        void beforeAsyncConnect() override
        {
            if (SessionBuilder<Derived>::mOption->dataHandler == nullptr)
            {
                throw std::runtime_error("data handler not setting");
            }
            settingEstablishHandle();
            beforeAsyncConnectOfTcpSessionBuilder();

            BaseSocketConnectBuilder<Derived>::beforeAsyncConnect();
        }

        void endAsyncConnect() override
        {
            auto newOption = std::make_shared<TcpSessionOption>();
            *newOption = *SessionBuilder<Derived>::mOption;
            BaseSessionBuilder<Derived>::mOption = newOption;

            BaseSocketConnectBuilder<Derived>::endAsyncConnect();
        }

        void        settingEstablishHandle()
        {
            internal::BaseSocketConnectBuilder<Derived>::mOption.establishHandler = 
                [option = SessionBuilder<Derived>::mOption](asio::ip::tcp::socket socket)
            {
                auto session = bsio::TcpSession::Make(
                    std::move(socket),
                    option->recvBufferSize,
                    option->dataHandler,
                    option->closedHandler);
                for (const auto& callback : option->establishHandler)
                {
                    callback(session);
                }
            };
        }

        virtual void    beforeAsyncConnectOfTcpSessionBuilder()
        {}
    };
} }
