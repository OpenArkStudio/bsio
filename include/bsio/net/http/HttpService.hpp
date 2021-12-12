#pragma once

#include <asio.hpp>
#include <bsio/net/TcpSession.hpp>
#include <bsio/net/http/HttpParser.hpp>
#include <bsio/net/http/WebSocketFormat.hpp>
#include <memory>
#include <thread>
#include <utility>

namespace bsio::net::http {

class HttpService;

class HttpSession : private asio::noncopyable
{
public:
    using Ptr = std::shared_ptr<HttpSession>;

    using EnterCallback = std::function<void(const HttpSession::Ptr&)>;
    using HttpParserCallback = std::function<void(const HTTPParser&, const HttpSession::Ptr&)>;
    using WsCallback = std::function<void(const HttpSession::Ptr&,
                                          WebSocketFormat::WebSocketFrameType opcode,
                                          const std::string& payload)>;

    using ClosedCallback = std::function<void(const HttpSession::Ptr&)>;
    using WsConnectedCallback = std::function<void(const HttpSession::Ptr&, const HTTPParser&)>;

public:
    HttpSession(
            TcpSession::Ptr session,
            HttpParserCallback parserCallback,
            WsCallback wsCallback,
            WsConnectedCallback wsConnectedCallback,
            ClosedCallback closedCallback)
        : mSession(std::move(session)),
          mHttpRequestCallback(std::move(parserCallback)),
          mWSCallback(std::move(wsCallback)),
          mWSConnectedCallback(std::move(wsConnectedCallback)),
          mCloseCallback(std::move(closedCallback))
    {
    }

    void setSession(TcpSession::Ptr session)
    {
        std::call_once(mOnce, [=]() {
            mSession = session;
        });
    }

    void send(const char* packet, size_t len, TcpSession::SendCompletedCallback&& callback = nullptr) const
    {
        mSession->send(std::string(packet, len), std::forward<TcpSession::SendCompletedCallback>(callback));
    }

    void send(std::string packet, TcpSession::SendCompletedCallback&& callback = nullptr) const
    {
        mSession->send(std::move(packet), std::forward<TcpSession::SendCompletedCallback>(callback));
    }

    void shutdown(asio::ip::tcp::socket::shutdown_type type) const
    {
        mSession->shutdown(type);
    }

    void close() const
    {
        mSession->close();
    }

    virtual ~HttpSession() = default;

    const TcpSession::Ptr& getSession() const
    {
        return mSession;
    }

    const HttpParserCallback& getHttpCallback() const
    {
        return mHttpRequestCallback;
    }

    const ClosedCallback& getCloseCallback() const
    {
        return mCloseCallback;
    }

    const WsCallback& getWSCallback() const
    {
        return mWSCallback;
    }

    const WsConnectedCallback& getWSConnectedCallback() const
    {
        return mWSConnectedCallback;
    }

private:
    std::once_flag mOnce;
    TcpSession::Ptr mSession;
    HttpParserCallback mHttpRequestCallback;
    WsCallback mWSCallback;
    ClosedCallback mCloseCallback;
    WsConnectedCallback mWSConnectedCallback;

    friend class HttpService;
};

class HttpService
{
public:
    static size_t ProcessWebSocket(const char* buffer,
                                   size_t len,
                                   const HTTPParser::Ptr& httpParser,
                                   const HttpSession::Ptr& httpSession)
    {
        size_t leftLen = len;

        const auto& wsCallback = httpSession->getWSCallback();
        auto& cacheFrame = httpParser->getWSCacheFrame();
        auto& parseString = httpParser->getWSParseString();

        while (leftLen > 0)
        {
            parseString.clear();

            auto opcode = WebSocketFormat::WebSocketFrameType::ERROR_FRAME;
            size_t frameSize = 0;
            bool isFin = false;

            if (!WebSocketFormat::wsFrameExtractBuffer(buffer,
                                                       leftLen,
                                                       parseString,
                                                       opcode,
                                                       frameSize,
                                                       isFin))
            {
                break;
            }

            if (!isFin ||
                opcode == WebSocketFormat::WebSocketFrameType::CONTINUATION_FRAME)
            {
                cacheFrame += parseString;
                parseString.clear();
            }
            if (!isFin &&
                opcode != WebSocketFormat::WebSocketFrameType::CONTINUATION_FRAME)
            {
                httpParser->cacheWSFrameType(opcode);
            }

            leftLen -= frameSize;
            buffer += frameSize;

            if (!isFin)
            {
                continue;
            }

            if (opcode == WebSocketFormat::WebSocketFrameType::CONTINUATION_FRAME)
            {
                if (!cacheFrame.empty())
                {
                    parseString = std::move(cacheFrame);
                    cacheFrame.clear();
                }
                opcode = httpParser->getWSFrameType();
            }

            if (wsCallback != nullptr)
            {
                wsCallback(httpSession, opcode, parseString);
            }
        }

        return (len - leftLen);
    }

    static size_t ProcessHttp(const char* buffer,
                              size_t len,
                              const HTTPParser::Ptr& httpParser,
                              const HttpSession::Ptr& httpSession)
    {
        size_t retlen = len;
        if (!httpParser->isCompleted())
        {
            retlen = httpParser->tryParse(buffer, len);
            if (!httpParser->isCompleted())
            {
                return retlen;
            }
        }

        if (httpParser->isWebSocket())
        {
            if (httpParser->hasKey("Sec-WebSocket-Key"))
            {
                auto response = WebSocketFormat::wsHandshake(
                        httpParser->getValue("Sec-WebSocket-Key"));
                httpSession->send(response.c_str(),
                                  response.size());
            }

            const auto& wsConnectedCallback = httpSession->getWSConnectedCallback();
            if (wsConnectedCallback != nullptr)
            {
                wsConnectedCallback(httpSession, *httpParser);
            }
        }
        else
        {
            const auto& httpCallback = httpSession->getHttpCallback();
            if (httpCallback != nullptr)
            {
                httpCallback(*httpParser, httpSession);
            }
        }

        return retlen;
    }
};

}// namespace bsio::net::http
