#pragma once

#include <algorithm>
#include <memory>
#include <functional>
#include <mutex>
#include <deque>
#include <cmath>
#include <iostream>

#include <asio/socket_base.hpp>
#include <asio.hpp>
#include <asio/ssl.hpp>

namespace bsio { namespace net {

    const size_t MinReceivePrepareSize = 1024;

    class BaseSocket
    {
    public:
        virtual ~BaseSocket() = default;

        virtual void handshake(std::function<void(const asio::error_code& error)>) = 0;
        virtual void shutdown(asio::socket_base::shutdown_type what) = 0;
        virtual void close() = 0;

        virtual void async_receive(asio::mutable_buffer,
                    std::function<void(std::error_code ec, size_t bytesTransferred)>) = 0;
        virtual void async_send(const std::vector<asio::const_buffer>&,
                    std::function<void(std::error_code ec, size_t bytesTransferred)>) = 0;

        virtual void post(std::function<void()>) = 0;
        virtual std::shared_ptr<asio::steady_timer> runAfter(std::chrono::nanoseconds timeout,
                    std::function<void(void)> callback) = 0;
    };

    class NormalTcpSocket : public BaseSocket
    {
    public:
        explicit NormalTcpSocket(asio::ip::tcp::socket socket)
            :
            mSocket(std::move(socket))
        {
            mSocket.non_blocking(true);
            mSocket.set_option(asio::ip::tcp::no_delay(true));
        }

    private:
        void handshake(std::function<void(const asio::error_code& error)> callback) override
        {
            asio::error_code e;
            callback(e);
        }
        void shutdown(asio::socket_base::shutdown_type what) override
        {
            mSocket.shutdown(what);
        }
        void close() override
        {
            mSocket.close();
        }
        void async_receive(asio::mutable_buffer buffer,
                           std::function<void(std::error_code c, size_t bytesTransferred)> callback) override
        {
            mSocket.async_receive(buffer, callback);
        }
        void async_send(const std::vector<asio::const_buffer>& buffer,
                        std::function<void(std::error_code, size_t)> callback) override
        {
            mSocket.async_write_some(buffer, callback);
        }
        void post(std::function<void()> callback) override
        {
            asio::post(mSocket.get_executor(), callback);
        }
        std::shared_ptr<asio::steady_timer> runAfter(std::chrono::nanoseconds timeout, std::function<void(void)> callback) override
        {
            auto timer = std::make_shared<asio::steady_timer>(mSocket.get_executor());
            timer->expires_from_now(timeout);
            timer->async_wait([callback = std::move(callback), timer](const asio::error_code & ec)
                              {
                                  if (!ec)
                                  {
                                      callback();
                                  }
                              });
            return timer;
        }

    private:
        asio::ip::tcp::socket               mSocket;
    };

    class SSLTcpSocket : public BaseSocket
    {
    public:
        SSLTcpSocket(asio::ip::tcp::socket socket,
                    asio::ssl::context context,
                    asio::ssl::stream_base::handshake_type type)
                    :
                    mSocket(std::move(socket), context)
        {
            mSocket.lowest_layer().non_blocking(true);
            mSocket.lowest_layer().set_option(asio::ip::tcp::no_delay(true));
        }

    private:
        void handshake(std::function<void(const asio::error_code& error)> callback) override
        {
            mSocket.async_handshake(mType, callback);
        }
        void shutdown(asio::socket_base::shutdown_type what) override
        {
            //mSocket.async_shutdown(nullptr);
        }
        void close() override
        {
        }
        void async_receive(asio::mutable_buffer buffer,
                           std::function<void(std::error_code ec, size_t bytesTransferred)> callback) override
        {
            mSocket.async_read_some(buffer, callback);
        }
        void async_send(const std::vector<asio::const_buffer>& buffer,
                        std::function<void(std::error_code ec, size_t bytesTransferred)> callback) override
        {
            mSocket.async_write_some(buffer, callback);
        }
        void post(std::function<void()> callback) override
        {
            asio::post(mSocket.get_executor(), callback);
        }
        std::shared_ptr<asio::steady_timer> runAfter(std::chrono::nanoseconds timeout,
                std::function<void(void)> callback) override
        {
            auto timer = std::make_shared<asio::steady_timer>(mSocket.get_executor());
            timer->expires_from_now(timeout);
            timer->async_wait([callback = std::move(callback), timer](const asio::error_code & ec)
                              {
                                  if (!ec)
                                  {
                                      callback();
                                  }
                              });
            return timer;
        }
    private:
        asio::ssl::stream<asio::ip::tcp::socket>    mSocket;
        asio::ssl::stream_base::handshake_type      mType;
    };

    class TcpSession :  public asio::noncopyable, 
                        public std::enable_shared_from_this<TcpSession>
    {
    public:
        using Ptr = std::shared_ptr<TcpSession>;
        using DataHandler = std::function<size_t(Ptr, const char*, size_t)>;
        using ClosedHandler = std::function<void(Ptr)>;
        using SendCompletedCallback = std::function<void()>;

        static Ptr Make(
            asio::ip::tcp::socket socket,
            size_t maxRecvBufferSize,
            DataHandler dataHandler,
            ClosedHandler closedHandler)
        {
            class make_shared_enabler : public TcpSession
            {
            public:
                make_shared_enabler(
                    asio::ip::tcp::socket socket,
                    size_t maxRecvBufferSize,
                    DataHandler dataHandler,
                    ClosedHandler closedHandler)
                    :
                    TcpSession(std::move(socket), 
                        maxRecvBufferSize, 
                        std::move(dataHandler),
                        std::move(closedHandler))
                {}
            };

            auto session = std::make_shared<make_shared_enabler>(
                std::move(socket), 
                maxRecvBufferSize, 
                std::move(dataHandler),
                std::move(closedHandler));

            session->startHandshake();

            return std::static_pointer_cast<TcpSession>(session);
        }

        virtual ~TcpSession() = default;

        auto    runAfter(std::chrono::nanoseconds timeout, std::function<void(void)> callback)
        {
            return mBaseSocket->runAfter(timeout, callback);
        }

        void    asyncSetDataHandler(DataHandler dataHandler)
        {
            mBaseSocket->post([self = shared_from_this(), this, dataHandler = std::move(dataHandler)]() mutable
                              {
                                  mDataHandler = std::move(dataHandler);
                                  tryProcessRecvBuffer();
                                  tryAsyncRecv();
                              });
        }

        void    postClose() noexcept
        {
            mBaseSocket->post([self = shared_from_this(), this]()
                              {
                                  mBaseSocket->close();
                              });
        }

        void    postShutdown(asio::ip::tcp::socket::shutdown_type type) noexcept
        {
            mBaseSocket->post([self = shared_from_this(), this, type]()
                              {
                                  mBaseSocket->shutdown(type);
                              });
        }

        void    send(std::shared_ptr<std::string> msg, SendCompletedCallback callback = nullptr) noexcept
        {
            {
                std::lock_guard<std::mutex> lck(mSendGuard);
                mPendingSendMsg.push_back({ 0, std::move(msg), std::move(callback) });
            }
            trySend();
        }

        void    send(std::string msg, SendCompletedCallback callback = nullptr) noexcept
        {
            send(std::make_shared<std::string>(std::move(msg)), std::move(callback));
        }

    private:
        //TODO::don't force use NormalTcpSocket
        TcpSession(
            asio::ip::tcp::socket socket,
            size_t maxRecvBufferSize,
            DataHandler dataHandler,
            ClosedHandler closedHandler)
            :
            mBaseSocket(std::make_shared<NormalTcpSocket>(std::move(socket))),
            mSending(false),
            mDataHandler(std::move(dataHandler)),
            mReceiveBuffer(std::max<size_t>(MinReceivePrepareSize, maxRecvBufferSize)),
            mCurrentPrepareSize(std::min<size_t>(MinReceivePrepareSize, maxRecvBufferSize)),
            mClosedHandler(std::move(closedHandler)),
            mCurrentTanhXDiff(0)
        {
        }

        // TODO::can't send data before handshake completed
        void    startHandshake()
        {
            mBaseSocket->handshake([self = shared_from_this(), this](const asio::error_code& error)
                                   {
                                       startRecv();
                                   });
        }

        void    startRecv()
        {
            std::call_once(mRecvInitOnceFlag,
                           [self = shared_from_this(), this]()
                           {
                               tryAsyncRecv();
                           });
        }

        void    tryAsyncRecv()
        {
            if(mInRecv)
            {
                return;
            }

            try
            {
                mBaseSocket->async_receive(mReceiveBuffer.prepare(mCurrentPrepareSize - mReceiveBuffer.in_avail()),
                                           [self = shared_from_this(), this](std::error_code ec, size_t bytesTransferred)
                                           {
                                               onRecvCompleted(ec, bytesTransferred);
                                           });
            }
            catch (const std::length_error& ec)
            {
                std::cout << "do recv, cause error of async receive:" << ec.what() << std::endl;
                //TODO::callback to user
            }
        }

        void    onRecvCompleted(std::error_code ec, size_t bytesTransferred)
        {
            mInRecv = false;

            if (ec)
            {
                //TODO::处理error code
                return;
            }

            if((mCurrentPrepareSize-mReceiveBuffer.in_avail()) == bytesTransferred)
            {
                const auto TanhXDiff = 0.2;

                const auto oldTanh = std::tanh(mCurrentTanhXDiff);
                mCurrentTanhXDiff += TanhXDiff;
                const auto newTanh = std::tanh(mCurrentTanhXDiff);
                const auto maxSizeDiff = mReceiveBuffer.max_size() -
                        std::min<size_t>(mReceiveBuffer.max_size(), MinReceivePrepareSize);
                const auto sizeDiff = maxSizeDiff * (newTanh-oldTanh);

                mCurrentPrepareSize += sizeDiff;
                mCurrentPrepareSize = std::min<size_t>(mCurrentPrepareSize, mReceiveBuffer.max_size());
            }
            mReceiveBuffer.commit(bytesTransferred);

            tryProcessRecvBuffer();

            tryAsyncRecv();
        }

        void    trySend()
        {
            std::lock_guard<std::mutex> lck(mSendGuard);
            if (mSending || mPendingSendMsg.empty())
            {
                return;
            }

            mBuffers.resize(mPendingSendMsg.size());
            for (std::size_t i = 0; i < mPendingSendMsg.size(); ++i)
            {
                auto& msg = mPendingSendMsg[i];
                mBuffers[i] = asio::const_buffer(msg.msg->c_str() + msg.sendPos,
                    msg.msg->size() - msg.sendPos);
            }

            mSending = true;
            mBaseSocket->async_send(mBuffers,
                    [self = shared_from_this(), this](std::error_code ec, size_t bytesTransferred)
                    {
                        onSendCompleted(ec, bytesTransferred);
                    });
        }

        void    onSendCompleted(std::error_code ec, size_t bytesTransferred)
        {
            std::vector<SendCompletedCallback> completedCallbacks;
            {
                std::lock_guard<std::mutex> lck(mSendGuard);
                mSending = false;
                if (ec)
                {
                    // TODO::错误回调
                    return;
                }
                completedCallbacks = adjustSendBuffer(bytesTransferred);
            }
            for (const auto& callback : completedCallbacks)
            {
                callback();
            }

            trySend();
        }

        std::vector<SendCompletedCallback>  adjustSendBuffer(size_t bytesTransferred)
        {
            std::vector<SendCompletedCallback> completedCallbacks;

            while (bytesTransferred > 0)
            {
                auto& frontMsg = mPendingSendMsg.front();
                const auto len = std::min<size_t>(bytesTransferred, frontMsg.msg->size() - frontMsg.sendPos);
                frontMsg.sendPos += len;
                bytesTransferred -= len;
                if (frontMsg.sendPos == frontMsg.msg->size())
                {
                    if (frontMsg.callback)
                    {
                        completedCallbacks.push_back(std::move(frontMsg.callback));
                    }
                    mPendingSendMsg.pop_front();
                }
            }

            return completedCallbacks;
        }

        void    tryProcessRecvBuffer()
        {
            if (mDataHandler == nullptr)
            {
                return;
            }

            const auto validReadBuffer = mReceiveBuffer.data();
            const auto procLen = mDataHandler(shared_from_this(),
                                              static_cast<const char* >(validReadBuffer.data()),
                                              validReadBuffer.size());
            assert(procLen <= validReadBuffer.size());
            if (procLen <= validReadBuffer.size())
            {
                mReceiveBuffer.consume(procLen);
            }
            else
            {
                ;//throw
            }
        }
    private:
        std::shared_ptr<BaseSocket>         mBaseSocket;

        // 同时只能发起一次send writev请求
        bool                                mSending;
        std::mutex                          mSendGuard;
        struct PendingMsg
        {
            size_t  sendPos;
            std::shared_ptr<std::string>    msg;
            SendCompletedCallback           callback;
        };

        std::deque<PendingMsg>              mPendingSendMsg;
        std::vector<asio::const_buffer>     mBuffers;

        std::once_flag                      mRecvInitOnceFlag;
        bool                                mInRecv = false;
        DataHandler                         mDataHandler;
        asio::streambuf                     mReceiveBuffer;
        size_t                              mCurrentPrepareSize;
        ClosedHandler                       mClosedHandler;
        double                              mCurrentTanhXDiff;
    };

    using TcpSessionEstablishHandler = std::function<void(TcpSession::Ptr)>;

} }