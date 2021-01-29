#pragma once

#include <algorithm>
#include <asio.hpp>
#include <asio/socket_base.hpp>
#include <bsio/SendableMsg.hpp>
#include <bsio/base/Packet.hpp>
#include <cmath>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>

namespace bsio::net {

const size_t MinReceivePrepareSize = 1024;

class TcpSession : public asio::noncopyable, public std::enable_shared_from_this<TcpSession>
{
public:
    using Ptr = std::shared_ptr<TcpSession>;
    using DataHandler = std::function<void(Ptr, bsio::base::BasePacketReader&)>;
    using ClosedHandler = std::function<void(Ptr)>;
    using SendCompletedCallback = std::function<void()>;
    using HighWaterCallback = std::function<void()>;

    static Ptr Make(asio::ip::tcp::socket socket,
                    size_t maxRecvBufferSize,
                    DataHandler dataHandler,
                    ClosedHandler closedHandler)
    {
        class make_shared_enabler : public TcpSession
        {
        public:
            make_shared_enabler(asio::ip::tcp::socket socket,
                                size_t maxRecvBufferSize,
                                DataHandler dataHandler,
                                ClosedHandler closedHandler)
                : TcpSession(
                          std::move(socket),
                          maxRecvBufferSize,
                          std::move(dataHandler),
                          std::move(closedHandler))
            {
            }
        };

        auto session = std::make_shared<make_shared_enabler>(
                std::move(socket), maxRecvBufferSize, std::move(dataHandler), std::move(closedHandler));

        session->startAsyncRecv();

        return std::static_pointer_cast<TcpSession>(session);
    }

    virtual ~TcpSession() = default;

    auto runAfter(std::chrono::nanoseconds timeout, std::function<void(void)> callback)
    {
        auto timer = std::make_shared<asio::steady_timer>(mSocket.get_executor());
        timer->expires_from_now(timeout);
        timer->async_wait([callback = std::move(callback), timer](const asio::error_code& ec) {
            if (!ec)
            {
                callback();
            }
        });
        return timer;
    }


    void asyncSetDataHandler(DataHandler dataHandler)
    {
        asio::dispatch(mSocket.get_executor(),
                       [self = shared_from_this(), this, dataHandler = std::move(dataHandler)]() mutable {
                           mDataHandler = std::move(dataHandler);
                           tryProcessRecvBuffer();
                           startAsyncRecv();
                       });
    }

    void asyncSetHighWater(HighWaterCallback callback, size_t highWater)
    {
        asio::dispatch(mSocket.get_executor(),
                       [self = shared_from_this(), this, callback = std::move(callback), highWater]() mutable {
                           mHighWaterCallback = std::move(callback);
                           mHighWater = highWater;
                       });
    }

    void postClose() noexcept
    {
        asio::dispatch(mSocket.get_executor(),
                       [self = shared_from_this(), this]() {
                           causeClosed();
                       });
    }

    void postShutdown(asio::ip::tcp::socket::shutdown_type type) noexcept
    {
        asio::dispatch(mSocket.get_executor(), [self = shared_from_this(), this, type]() {
            if (mSocket.is_open())
            {
                try
                {
                    mSocket.shutdown(type);
                }
                catch (...)
                {
                }
            }
        });
    }

    void postShrinkReceiveBuffer()
    {
        asio::dispatch(mSocket.get_executor(),
                       [self = shared_from_this(), this]() {
                           mNeedShrinkReceiveBuffer = true;
                       });
    }

    void send(SendableMsg::Ptr msg, SendCompletedCallback callback = nullptr) noexcept
    {
        if (!mSocket.is_open())
        {
            return;
        }
        {
            std::lock_guard<std::mutex> lck(mSendGuard);
            mSendingSize += msg->size();
            mPendingSendMsgList.emplace_back(std::move(msg), std::move(callback));

            if (mSendingSize > mHighWater && mHighWaterCallback != nullptr)
            {
                // prevent send data in high water callback, so use post defer execute.
                asio::post(mSocket.get_executor(),
                           [self = shared_from_this(), this]() {
                               mHighWaterCallback();
                           });
            }
            if (mSending)
            {
                return;
            }
        }
        trySend();
    }

    void send(std::string msg, SendCompletedCallback callback = nullptr) noexcept
    {
        send(MakeStringMsg(std::move(msg)), std::move(callback));
    }

private:
    TcpSession(asio::ip::tcp::socket socket,
               size_t maxRecvBufferSize,
               DataHandler dataHandler,
               ClosedHandler closedHandler)
        : mSocket(std::move(socket)),
          mDataHandler(std::move(dataHandler)),
          mReceiveBuffer(
                  std::make_unique<asio::streambuf>(std::max<size_t>(MinReceivePrepareSize, maxRecvBufferSize))),
          mClosedHandler(std::move(closedHandler))
    {
        mSocket.non_blocking(true);
        mSocket.set_option(asio::ip::tcp::no_delay(true));
    }

    void growReceiveBuffer()
    {
        const auto TanhXDiff = 0.2;

        const auto oldTanh = std::tanh(mCurrentTanhXDiff);
        mCurrentTanhXDiff += TanhXDiff;
        const auto newTanh = std::tanh(mCurrentTanhXDiff);
        const auto sizeDiff = mReceiveBuffer->max_size() * (newTanh - oldTanh);

        const auto newCapacity =
                std::min<size_t>(mReceiveBuffer->capacity() + static_cast<size_t>(sizeDiff), mReceiveBuffer->max_size());
        mReceiveBuffer->prepare(newCapacity - mReceiveBuffer->data().size());
        mReceivePos = mReceiveBuffer->data().size();
    }

    void moveReceiveBuffer()
    {
        mReceiveBuffer->prepare(maxValidReceiveBufferSize() - mReceiveBuffer->data().size());
        mReceivePos = mReceiveBuffer->data().size();
    }

    void adjustReceiveBuffer()
    {
        if (mReceiveBuffer->data().size() == 0)
        {
            mReceivePos = 0;
        }
        if (maxValidReceiveBufferSize() > mReceivePos)
        {
            return;
        }

        if (maxValidReceiveBufferSize() == mReceiveBuffer->data().size())
        {
            growReceiveBuffer();
        }
        else
        {
            moveReceiveBuffer();
        }
    }

    size_t maxValidReceiveBufferSize()
    {
        return std::min(mReceiveBuffer->capacity(), mReceiveBuffer->max_size());
    }

    void startAsyncRecv()
    {
        if (mRecvPosted)
        {
            return;
        }

        adjustReceiveBuffer();
        try
        {
            const auto buffer = mReceiveBuffer->prepare(maxValidReceiveBufferSize() - mReceivePos);
            if (buffer.size() == 0)
            {
                throw std::runtime_error("buffer size is zero");
            }
            mSocket.async_receive(
                    buffer,
                    [self = shared_from_this(), this](std::error_code ec, size_t bytesTransferred) {
                        onRecvCompleted(ec, bytesTransferred);
                    });
            mRecvPosted = true;
        }
        catch (const std::length_error& ec)
        {
            std::cout << "do recv, cause error of async receive:" << ec.what() << std::endl;
            // TODO::callback to user
        }
        catch (const std::runtime_error& ec)
        {
            std::cout << ec.what() << std::endl;
        }
    }

    void onRecvCompleted(std::error_code ec, size_t bytesTransferred)
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

        if (maxValidReceiveBufferSize() == bytesTransferred)
        {
            growReceiveBuffer();
        }

        startAsyncRecv();
    }

    void trySend()
    {
        {
            std::lock_guard<std::mutex> lck(mSendGuard);
            if (mSending || mPendingSendMsgList.empty())
            {
                return;
            }
            std::swap(mSendingMsgList, mPendingSendMsgList);
            mSending = true;
        }
        asio::dispatch(mSocket.get_executor(),
                       [self = shared_from_this(), this]() {
                           flush();
                       });
    }

    void flush()
    {
        {
            std::lock_guard<std::mutex> lck(mSendGuard);
            mBuffers.clear();
            //mBuffers.reserve(mSendingMsgList.size());
            //mBuffers.resize(mSendingMsgList.size());
            for (std::size_t i = 0; i < mSendingMsgList.size(); ++i)
            {
                auto& msg = mSendingMsgList[i];
                mBuffers.emplace_back(static_cast<const char*>(msg.msg->data()), msg.msg->size());
                //mBuffers[i] = asio::const_buffer(static_cast<const char*>(msg.msg->data()), msg.msg->size());
            }
        }
        asio::async_write(mSocket, mBuffers,
                          [self = shared_from_this(), this](std::error_code ec, size_t bytesTransferred) {
                              onSendCompleted(ec, bytesTransferred);
                          });
    }

    void onSendCompleted(std::error_code ec, size_t bytesTransferred)
    {
        if (ec)
        {
            causeClosed();
            return;
        }

        for(const auto& msg : mSendingMsgList)
        {
            if(msg.callback)
            {
                msg.callback();
            }
        }
        mSendingMsgList.clear();

        {
            std::lock_guard<std::mutex> lck(mSendGuard);
            mSending = false;
            mSendingSize -= bytesTransferred;
        }

        trySend();
    }

    void tryProcessRecvBuffer()
    {
        if (mDataHandler == nullptr)
        {
            return;
        }

        const auto validReadBuffer = mReceiveBuffer->data();
        auto reader = bsio::base::BasePacketReader(static_cast<const char*>(validReadBuffer.data()),
                                                   validReadBuffer.size(),
                                                   false);
        mDataHandler(shared_from_this(), reader);
        const auto consumedLen = reader.savedPos();
        assert(consumedLen <= validReadBuffer.size());
        if (consumedLen <= validReadBuffer.size())
        {
            mReceiveBuffer->consume(consumedLen);
        }
        else
        {
            ;// throw
        }
    }

    void checkNeedShrinkReceiveBuffer()
    {
        if (mNeedShrinkReceiveBuffer)
        {
            shrinkReceiveBuffer();
            mNeedShrinkReceiveBuffer = false;
        }
    }

    void shrinkReceiveBuffer()
    {
        if (mRecvPosted)
        {
            return;
        }
        const auto validReadBuffer = mReceiveBuffer->data();
        std::unique_ptr<asio::streambuf> tmp = std::make_unique<asio::streambuf>(mReceiveBuffer->max_size());
        tmp->prepare(validReadBuffer.size());
        tmp->commit(tmp->sputn(static_cast<const char*>(validReadBuffer.data()), validReadBuffer.size()));
        mReceivePos = tmp->data().size();
        mReceiveBuffer = std::move(tmp);
    }

    void causeClosed()
    {
        try
        {
            // already closed
            if (!mSocket.is_open())
            {
                return;
            }

            mSocket.close();
            if (mClosedHandler != nullptr)
            {
                mClosedHandler(shared_from_this());
                mClosedHandler = nullptr;
            }
            mDataHandler = nullptr;
        }
        catch (...)
        {
        }
    }

private:
    asio::ip::tcp::socket mSocket;

    // 同时只能发起一次send writev请求
    bool mSending = false;
    std::mutex mSendGuard;
    struct PendingMsg {
        PendingMsg() = default;
        PendingMsg(SendableMsg::Ptr m, SendCompletedCallback c)
            : msg(std::move(m)),
              callback(std::move(c))
        {}
        SendableMsg::Ptr msg;
        SendCompletedCallback callback;
    };

    std::vector<PendingMsg> mSendingMsgList;
    std::vector<PendingMsg> mPendingSendMsgList;
    std::vector<asio::const_buffer> mBuffers;
    size_t mSendingSize = 0;
    HighWaterCallback mHighWaterCallback;
    size_t mHighWater = 16 * 1024 * 1024;

    bool mRecvPosted = false;
    DataHandler mDataHandler;
    std::unique_ptr<asio::streambuf> mReceiveBuffer;
    size_t mReceivePos = 0;
    ClosedHandler mClosedHandler;
    double mCurrentTanhXDiff = 0;
    bool mNeedShrinkReceiveBuffer = false;
};

using TcpSessionEstablishHandler = std::function<void(TcpSession::Ptr)>;

}// namespace bsio::net
