#pragma once

#include <bsio/http/HttpService.hpp>
#include <bsio/wrapper/AcceptorBuilder.hpp>
#include <bsio/wrapper/internal/HttpSessionBuilder.hpp>

namespace bsio {

    class HttpAcceptorBuilder : public internal::BaseSocketAcceptorBuilder<HttpAcceptorBuilder>,
                                public internal::BaseHttpBuilder<HttpAcceptorBuilder>
    {
    protected:
        void beforeStartAccept() override
        {
            BaseSocketAcceptorBuilder<HttpAcceptorBuilder>::mOption.establishHandler =
                [option = internal::BaseSessionBuilder<HttpAcceptorBuilder>::mOption](asio::ip::tcp::socket socket)
            {
                const auto session = TcpSession::Make(std::move(socket),
                    option->recvBufferSize,
                    option->dataHandler,
                    option->closedHandler);
                for (const auto& callback : option->establishHandler)
                {
                    callback(session);
                }
            };

            setupHttp();
        }

        void endStartAccept() override
        {
            const auto newOption = std::make_shared<internal::TcpSessionOption>();
            *newOption = *BaseSessionBuilder<HttpAcceptorBuilder>::mOption;
            BaseSessionBuilder<HttpAcceptorBuilder>::mOption = newOption;
        }
    };

}