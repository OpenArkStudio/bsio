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

#include <bsio/SendableMsg.hpp>

namespace bsio { namespace net {

    const size_t MinReceivePrepareSize = 1024;

    class TcpSession :  public asio::noncopyable, 
                        public std::enable_shared_from_this<TcpSession>
    {
    public:
        using Ptr = std::shared_ptr<TcpSession>;
        using DataHandler = std::function<size_t(Ptr, const char*, size_t)>;
        using ClosedHandler = std::function<void(Ptr)>;
        using SendCompletedCallback = std::function<void()>;
        using HighWaterCallback = std::function<void()>;

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

            session->tryAsyncRecv();

            return std::static_pointer_cast<TcpSession>(session);
        }

        virtual ~TcpSession() = default;

        auto    runAfter(std::chrono::nanoseconds timeout, std::function<void(void)> callback)
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

        void    asyncSetDataHandler(DataHandler dataHandler)
        {
            asio::post(mSocket.get_executor(),
                       [self = shared_from_this(), this, dataHandler = std::move(dataHandler)]() mutable
                       {
                            mDataHandler = std::move(dataHandler);
                            tryProcessRecvBuffer();
                            tryAsyncRecv();
                       });
        }

        void    asyncSetHighWater(HighWaterCallback callback, size_t highWater)
        {
            asio::post(mSocket.get_executor(),
                       [self = shared_from_this(), this, callback = std::move(callback), highWater]() mutable
                       {
                           mHighWaterCallback = std::move(callback);
                           mHighWater = highWater;
                       });
        }

        void    postClose() noexcept
        {
            asio::post(mSocket.get_executor(),
                       [self = shared_from_this(), this]()
                       {
                           mSocket.close();
                       });
        }

        void    postShutdown(asio::ip::tcp::socket::shutdown_type type) noexcept
        {
            asio::post(mSocket.get_executor(),
                       [self = shared_from_this(), this, type]()
                       {
                           //TODO::maybe need try shutdown
                           if(mSocket.is_open())
                           {
                               mSocket.shutdown(type);
                           }
                       });
        }

        void    postShrinkReceiveBuffer()
        {
            asio::post(mSocket.get_executor(),
                       [self = shared_from_this(), this]()
                       {
                           this->mNeedShrinkReceiveBuffer = true;
                       });
        }

        void    send(SendableMsg::Ptr msg, SendCompletedCallback callback = nullptr) noexcept
        {
            // TODO：：cache it's open status in this class;
            if(!mSocket.is_open())
            {
                return;
            }
            {
                std::lock_guard<std::mutex> lck(mSendGuard);
                mSendingSize += msg->size();
                mPendingSendMsg.push_back({ 0, std::move(msg), std::move(callback) });

                if(mSendingSize > mHighWater && mHighWaterCallback != nullptr)
                {
                    asio::post(mSocket.get_executor(),
                               [self = shared_from_this(), this]()
                               {
                                   mHighWaterCallback();
                               });
                }
            }
            trySend();
        }

        void    send(std::string msg, SendCompletedCallback callback = nullptr) noexcept
        {
            send(MakeStringMsg(std::move(msg)), std::move(callback));
        }

    private:
        TcpSession(
            asio::ip::tcp::socket socket,
            size_t maxRecvBufferSize,
            DataHandler dataHandler,
            ClosedHandler closedHandler)
            :
                mSocket(std::move(socket)),
                mSending(false),
                mDataHandler(std::move(dataHandler)),
                mReceiveBuffer(std::make_unique<asio::streambuf>(std::max<size_t>(MinReceivePrepareSize, maxRecvBufferSize))),
                mClosedHandler(std::move(closedHandler)),
                mCurrentTanhXDiff(0)
        {
            mSocket.non_blocking(true);
            mSocket.set_option(asio::ip::tcp::no_delay(true));
        }

        void    growReceiveBuffer()
        {
            const auto TanhXDiff = 0.2;

            const auto oldTanh = std::tanh(mCurrentTanhXDiff);
            mCurrentTanhXDiff += TanhXDiff;
            const auto newTanh = std::tanh(mCurrentTanhXDiff);
            const auto sizeDiff = mReceiveBuffer->max_size() * (newTanh-oldTanh);

            const auto newCapacity = std::min<size_t>(mReceiveBuffer->capacity() + sizeDiff,
                                           mReceiveBuffer->max_size());
            mReceiveBuffer->prepare(newCapacity-mReceiveBuffer->data().size());
            mReceivePos = mReceiveBuffer->data().size();
        }

        void    moveReceiveBuffer()
        {
            mReceiveBuffer->prepare(maxValidReceiveBufferSize() - mReceiveBuffer->data().size());
            mReceivePos = mReceiveBuffer->data().size();
        }

        void    adjustReceiveBuffer()
        {
            if(mReceiveBuffer->data().size() == 0)
            {
                mReceivePos = 0;
            }
            if(maxValidReceiveBufferSize() > mReceivePos)
            {
                return;
            }

            if(maxValidReceiveBufferSize() == mReceiveBuffer->data().size())
            {
                growReceiveBuffer();
            }
            else
            {
                moveReceiveBuffer();
            }
        }

        size_t  maxValidReceiveBufferSize()
        {
            return std::min(mReceiveBuffer->capacity(), mReceiveBuffer->max_size());
        }

        void    tryAsyncRecv()
        {
            if(mRecvPosted)
            {
                return;
            }

            adjustReceiveBuffer();
            try
            {
                auto buffer = mReceiveBuffer->prepare(maxValidReceiveBufferSize() - mReceivePos);
                if (buffer.size() == 0)
                {
                    throw std::runtime_error("buffer size is zero");
                }
                mSocket.async_receive(
                        buffer,
                        [self = shared_from_this(), this](std::error_code ec, size_t bytesTransferred)
                        {
                            onRecvCompleted(ec, bytesTransferred);
                        });
                mRecvPosted = true;
            }
            catch (const std::length_error& ec)
            {
                std::cout << "do recv, cause error of async receive:" << ec.what() << std::endl;
                //TODO::callback to user
            }
            catch(const std::runtime_error& ec)
            {
                std::cout << ec.what() << std::endl;
            }
        }

        void    onRecvCompleted(std::error_code ec, size_t bytesTransferred)
        {
            mRecvPosted = false;
            if (ec)
            {
                causeClosed();
                return;
            }

            mReceiveBuffer->commit(bytesTransferred);
            mReceivePos += bytesTransferred;

            tryProcessRecvBuffer();
            checkNeedShrinkReceiveBuffer();

            if(maxValidReceiveBufferSize() == bytesTransferred)
            {
                growReceiveBuffer();
            }

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
                mBuffers[i] = asio::const_buffer(static_cast<const char*>(msg.msg->data()) + msg.sendPos,
                    msg.msg->size() - msg.sendPos);
            }

            mSending = true;
            mSocket.async_send(mBuffers,
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
                completedCallbacks = adjustSendBuffer(bytesTransferred);
            }

            if (ec)
            {
                causeClosed();
                return;
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
                    mSendingSize -= frontMsg.sendPos;
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

            const auto validReadBuffer = mReceiveBuffer->data();
            const auto procLen = mDataHandler(shared_from_this(),
                                              static_cast<const char* >(validReadBuffer.data()),
                                              validReadBuffer.size());
            assert(procLen <= validReadBuffer.size());
            if (procLen <= validReadBuffer.size())
            {
                mReceiveBuffer->consume(procLen);
            }
            else
            {
                ;//throw
            }
        }

        void    checkNeedShrinkReceiveBuffer()
        {
            if(mNeedShrinkReceiveBuffer)
            {
                shrinkReceiveBuffer();
                mNeedShrinkReceiveBuffer = false;
            }
        }

        void    shrinkReceiveBuffer()
        {
            if(mRecvPosted)
            {
                return;
            }
            auto validReadBuffer = mReceiveBuffer->data();
            std::unique_ptr<asio::streambuf> tmp = std::make_unique<asio::streambuf>(mReceiveBuffer->max_size());
            tmp->prepare(validReadBuffer.size());
            tmp->commit(tmp->sputn(static_cast<const char*>(validReadBuffer.data()),
                                   validReadBuffer.size()));
            mReceivePos = tmp->data().size();
            mReceiveBuffer = std::move(tmp);
        }

        void    causeClosed()
        {
            // already closed
            if(!mSocket.is_open())
            {
                return;
            }

            mSocket.close();
            if(mClosedHandler != nullptr)
            {
                mClosedHandler(shared_from_this());
            }
        }
    private:
        asio::ip::tcp::socket               mSocket;

        // 同时只能发起一次send writev请求
        bool                                mSending;
        std::mutex                          mSendGuard;
        struct PendingMsg
        {
            size_t                  sendPos;
            SendableMsg::Ptr        msg;
            SendCompletedCallback   callback;
        };

        std::deque<PendingMsg>              mPendingSendMsg;
        std::vector<asio::const_buffer>     mBuffers;
        size_t                              mSendingSize = 0;
        HighWaterCallback                   mHighWaterCallback;
        size_t                              mHighWater = 16*1024*1024;

        bool                                mRecvPosted = false;
        DataHandler                         mDataHandler;
        std::unique_ptr<asio::streambuf>    mReceiveBuffer;
        size_t                              mReceivePos = 0;
        ClosedHandler                       mClosedHandler;
        double                              mCurrentTanhXDiff = 0;
        bool                                mNeedShrinkReceiveBuffer;
    };

    using TcpSessionEstablishHandler = std::function<void(TcpSession::Ptr)>;

} }