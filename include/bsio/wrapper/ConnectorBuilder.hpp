#pragma once

#include <memory>
#include <functional>

#include <bsio/TcpSession.hpp>
#include <bsio/wrapper/internal/SessionBuilder.hpp>
#include <bsio/wrapper/internal/ConnectorBuilder.hpp>
#include <utility>

namespace bsio {
    
    class SocketConnectBuilder : public internal::SocketConnectBuilderWithEstablishHandler<SocketConnectBuilder>
    {
    };

    class TcpSessionConnectBuilder : public internal::BaseTcpSessionConnectBuilder<TcpSessionConnectBuilder, internal::BaseSessionBuilderWithEnter>
    {
    public:
        TcpSessionConnectBuilder& WithDataHandler(TcpSession::DataHandler handler)
        {
            BaseSessionBuilderWithEnter<TcpSessionConnectBuilder>::mOption->dataHandler = std::move(handler);
            return *this;
        }
    };

}
