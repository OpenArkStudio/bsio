#pragma once

#include <memory>
#include <functional>

#include <bsio/TcpSession.hpp>
#include <bsio/wrapper/internal/SessionBuilder.hpp>
#include <bsio/wrapper/internal/ConnectorBuilder.hpp>

namespace bsio {
    
    class SocketConnectBuilder : public internal::SocketConnectBuilderWithEstablishHandler<SocketConnectBuilder>
    {
    };

    class TcpSessionConnectBuilder : public internal::BaseTcpSessionConnectBuilder<TcpSessionConnectBuilder, internal::BaseSessionBuilder>
    {
    public:
        TcpSessionConnectBuilder& WithDataHandler(TcpSession::DataHandler handler)
        {
            BaseSessionBuilder<TcpSessionConnectBuilder>::mOption->dataHandler = handler;
            return *this;
        }
    };

}
