#pragma once

#include <bsio/Functor.hpp>
#include <bsio/wrapper/internal/Option.hpp>
#include <bsio/http/HttpService.hpp>

namespace bsio { namespace net { namespace wrapper { namespace common {

    inline SocketEstablishHandler generateHttpEstablishHandler(
            internal::TcpSessionOption option,
            http::HttpSession::EnterCallback enterCallback,
            http::HttpSession::HttpParserCallback parserCallback,
            http::HttpSession::WsCallback wsCallback)
    {
        return [option = std::move(option),
                parserCallback = std::move(parserCallback),
                wsCallback = std::move(wsCallback),
                enterCallback = std::move(enterCallback)](asio::ip::tcp::socket socket)
        {
            auto httpSession = std::make_shared<http::HttpSession>();

            httpSession->setHttpCallback(parserCallback);
            httpSession->setWSCallback(wsCallback);

            auto httpParser = std::make_shared<http::HTTPParser>(HTTP_BOTH);
            auto dataHandler = [=](const TcpSession::Ptr& session, const char* buffer, size_t len)
            {
                (void)session;

                if (httpParser->isWebSocket())
                {
                    return http::HttpService::ProcessWebSocket(buffer,
                                                               len,
                                                               httpParser,
                                                               httpSession);
                }
                return http::HttpService::ProcessHttp(buffer, len, httpParser, httpSession);
            };

            const auto session = TcpSession::Make(std::move(socket),
                                                  option.recvBufferSize,
                                                  dataHandler,
                                                  option.closedHandler);

            httpSession->setSession(session);
            if (enterCallback != nullptr)
            {
                enterCallback(httpSession);
            }
        };
    }

} } } }