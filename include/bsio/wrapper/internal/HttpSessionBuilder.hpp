#pragma once

#include <utility>
#include <bsio/http/HttpService.hpp>
#include <bsio/wrapper/internal/SessionBuilder.hpp>

namespace bsio { namespace internal {

    template<typename Derived>
    class BaseHttpBuilder : public BaseSessionBuilder<Derived>
    {
    public:
        Derived& WithEnterCallback(bsio::net::http::HttpSession::EnterCallback callback)
        {
            mEnterCallback = std::move(callback);
            return static_cast<Derived&>(*this);
        }

        Derived& WithParserCallback(bsio::net::http::HttpSession::HttpParserCallback callback)
        {
            mParserCallback = std::move(callback);
            return static_cast<Derived&>(*this);
        }

        Derived& WithWsCallback(bsio::net::http::HttpSession::WsCallback handler)
        {
            mWsCallback = std::move(handler);
            return static_cast<Derived&>(*this);
        }

    protected:
        void setupHttp()
        {
            auto httpSession = std::make_shared<bsio::net::http::HttpSession>();

            httpSession->setHttpCallback(mParserCallback);
            httpSession->setWSCallback(mWsCallback);

            auto httpParser = std::make_shared<bsio::net::http::HTTPParser>(HTTP_BOTH);
            auto dataHandler = [=](const TcpSession::Ptr& session, const char* buffer, size_t len) {
                (void)session;
                size_t retlen = 0;

                if (httpParser->isWebSocket())
                {
                    retlen = bsio::net::http::HttpService::ProcessWebSocket(buffer,
                        len,
                        httpParser,
                        httpSession);
                }
                else
                {
                    retlen = bsio::net::http::HttpService::ProcessHttp(buffer,
                        len,
                        httpParser,
                        httpSession);
                }

                return retlen;
            };

            BaseSessionBuilder<Derived>::mOption->dataHandler = dataHandler;
            BaseSessionBuilder<Derived>::mOption->establishHandler.push_back(
                [callback = mEnterCallback, httpSession](TcpSession::Ptr session)
                {
                    httpSession->setSession(std::move(session));
                    callback(httpSession);
                });
        }

    private:
        bsio::net::http::HttpSession::EnterCallback mEnterCallback;
        bsio::net::http::HttpSession::HttpParserCallback mParserCallback;
        bsio::net::http::HttpSession::WsCallback    mWsCallback;
    };

} }
