#pragma once

#include <memory>

#include <asio.hpp>
#include <bsio/TcpSession.hpp>
#include <bsio/http/HttpParser.hpp>
#include <bsio/http/WebSocketFormat.hpp>
#include <utility>

namespace bsio { namespace net { namespace http {

    class HttpService;

    class HttpSession : public asio::noncopyable
    {
    public:
        using Ptr = std::shared_ptr<HttpSession>;

        using EnterCallback = std::function <void(const HttpSession::Ptr&)>;
        using HttpParserCallback = std::function <void(const bsio::net::http::HTTPParser&, const HttpSession::Ptr&)>;
        using WsCallback =  std::function < void(   const HttpSession::Ptr&,
                                                    bsio::net::http::WebSocketFormat::WebSocketFrameType opcode,
                                                    const std::string& payload)>;

        using ClosedCallback = std::function <void(const HttpSession::Ptr&)>;
        using WsConnectedCallback = std::function <void(const HttpSession::Ptr&, const bsio::net::http::HTTPParser&)>;

    public:
        void                        setHttpCallback(HttpParserCallback callback)
        {
            mHttpRequestCallback = std::move(callback);
        }

        void                        setClosedCallback(ClosedCallback callback)
        {
            mCloseCallback = std::move(callback);
        }

        void                        setWSCallback(WsCallback callback)
        {
            mWSCallback = std::move(callback);
        }

        void                        setWSConnected(WsConnectedCallback&& callback)
        {
            mWSConnectedCallback = std::move(callback);
        }

        void                        send(const char* packet,
            size_t len,
            bsio::TcpSession::SendCompletedCallback&& callback = nullptr)
        {
            mSession->send(std::make_shared<std::string>(packet, len), std::forward<bsio::TcpSession::SendCompletedCallback>(callback));
        }

        void                        postShutdown(asio::ip::tcp::socket::shutdown_type type) const
        {
            mSession->postShutdown(type);
        }

        void                        postClose() const
        {
            mSession->postClose();
        }

        virtual ~HttpSession() = default;

        void setSession(bsio::TcpSession::Ptr session)
        {
            mSession = std::move(session);
        }

        const bsio::TcpSession::Ptr& getSession() const
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
        bsio::TcpSession::Ptr       mSession;
        HttpParserCallback          mHttpRequestCallback;
        WsCallback                  mWSCallback;
        ClosedCallback              mCloseCallback;
        WsConnectedCallback         mWSConnectedCallback;

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
                    // 如果没有解析出完整的ws frame则退出函数
                    break;
                }

                // 如果当前fram的fin为false或者opcode为延续包
                // 则将当前frame的payload添加到cache
                if (!isFin || 
                    opcode == WebSocketFormat::WebSocketFrameType::CONTINUATION_FRAME)
                {
                    cacheFrame += parseString;
                    parseString.clear();
                }
                // 如果当前fram的fin为false，并且opcode不为延续包
                // 则表示收到分段payload的第一个段(frame)，需要缓存当前frame的opcode
                if (!isFin && 
                    opcode != bsio::net::http::WebSocketFormat::WebSocketFrameType::CONTINUATION_FRAME)
                {
                    httpParser->cacheWSFrameType(opcode);
                }

                leftLen -= frameSize;
                buffer += frameSize;

                if (!isFin)
                {
                    continue;
                }

                // 如果fin为true，并且opcode为延续包
                // 则表示分段payload全部接受完毕
                // 因此需要获取之前第一次收到分段frame的opcode作为整个payload的类型
                if (opcode == bsio::net::http::WebSocketFormat::WebSocketFrameType::CONTINUATION_FRAME)
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
            const bsio::net::http::HTTPParser::Ptr& httpParser,
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
                    auto response = bsio::net::http::WebSocketFormat::wsHandshake(
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
                if (httpParser->isKeepAlive())
                {
                    httpParser->clearParse();
                }
            }

            return retlen;
        }
    };

} } }
