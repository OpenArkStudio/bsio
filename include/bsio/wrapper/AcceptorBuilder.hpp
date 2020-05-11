#pragma once

#include <bsio/TcpAcceptor.hpp>
#include <bsio/wrapper/internal/SessionBuilder.hpp>
#include <bsio/wrapper/internal/AcceptorBuilder.hpp>

namespace bsio {

    class SocketAcceptorBuilder : public internal::BaseServerSocketBuilderWithEstablishHandler<SocketAcceptorBuilder>
    {
    };

    class SessionAcceptorBuilder :  public internal::BaseSocketAcceptorBuilder<SessionAcceptorBuilder>,
                                    public internal::SessionBuilderWithDataHandler<SessionAcceptorBuilder>
    {
    protected:
        void beforeStartAccept() override
        {
            // setting establishHandler
            BaseSocketAcceptorBuilder<SessionAcceptorBuilder>::mOption.establishHandler =
                [option = SessionBuilderWithDataHandler<SessionAcceptorBuilder>::mOption](asio::ip::tcp::socket socket)
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
        }

        void endStartAccept() override
        {
            const auto newOption = std::make_shared<internal::TcpSessionOption>();
            *newOption = *SessionBuilderWithDataHandler<SessionAcceptorBuilder>::mOption;
            SessionBuilderWithDataHandler<SessionAcceptorBuilder>::mOption = newOption;
        }
    };

}