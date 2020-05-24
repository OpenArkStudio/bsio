#pragma once

#include <bsio/wrapper/ConnectorBuilder.hpp>
#include <bsio/wrapper/internal/HttpSessionBuilder.hpp>

namespace bsio {

    class HttpConnectionBuilder : public internal::BaseTcpSessionConnectBuilder<HttpConnectionBuilder, internal::BaseHttpBuilder>
    {
    private:
        void beforeAsyncConnectOfTcpSessionBuilder() final
        {
            setupHttp();
        }
    };

}
