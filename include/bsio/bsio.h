#pragma once

#include <algorithm>
#include <memory>

#include <asio/ip/tcp.hpp>
#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>
#include <asio/deadline_timer.hpp>
#include <asio/io_service.hpp>

#include <brynet/utils/buffer.h>

namespace bsio {

    using namespace asio;
    using namespace asio::ip;

    class SharedSocket
    {
    public:
        using Ptr = std::shared_ptr<SharedSocket>;

        static SharedSocket::Ptr Make(tcp::socket socket, asio::io_context& ioContext)
        {
            return std::make_shared<SharedSocket>(std::move(socket), ioContext);
        }

        SharedSocket(tcp::socket socket, asio::io_context& ioContext)
            :
            mSocket(std::move(socket)),
            mIoContext(ioContext)
        {
        }

        virtual ~SharedSocket() = default;

        tcp::socket& socket()
        {
            return mSocket;
        }

        asio::io_context& context()
        {
            return mIoContext;
        }

    private:
        tcp::socket         mSocket;
        asio::io_context&   mIoContext;
    };

    class WrapperIoContext : public asio::noncopyable
    {
    public:
        using Ptr = std::shared_ptr<WrapperIoContext>;

        WrapperIoContext(int concurrencyHint)
            :
            mTrickyIoContext(std::make_shared<asio::io_context>(concurrencyHint)),
            mIoContext(*mTrickyIoContext)
        {
        }

        WrapperIoContext(asio::io_context& ioContext)
            :
            mIoContext(ioContext)
        {}

        virtual ~WrapperIoContext()
        {
            stop();
        }

        void    run()
        {
            asio::io_service::work worker(mIoContext);
            for (; !mIoContext.stopped();)
            {
                mIoContext.run();
            }
        }

        void    stop()
        {
            mIoContext.stop();
        }

        asio::io_context& context()
        {
            return mIoContext;
        }

        void    runAfter(std::chrono::nanoseconds timeout, std::function<void(void)> callback)
        {
            auto timer = std::make_shared<asio::steady_timer>(mIoContext);
            timer->expires_from_now(timeout);
            timer->async_wait([timer, callback](const asio::error_code & ec) {
                    if (!ec)
                    {
                        callback();
                    }
                });
        }

    public:
        std::shared_ptr<asio::io_context>   mTrickyIoContext;
        asio::io_context&                   mIoContext;
    };

    class IoContextThread : public asio::noncopyable
    {
    public:
        using Ptr = std::shared_ptr<IoContextThread>;

        IoContextThread(int concurrencyHint)
            :
            mWrapperIoContext(concurrencyHint)
        {
        }

        virtual ~IoContextThread()
        {
            stop();
        }

        void    start(size_t threadNum)
        {
            std::lock_guard<std::mutex> lck(mIoThreadGuard);
            if (threadNum == 0)
            {
                throw std::runtime_error("thread num is zero");
            }
            if (!mIoThreads.empty())
            {
                return;
            }
            for (size_t i = 0; i < threadNum; i++)
            {
                mIoThreads.push_back(std::thread([this]() {
                        mWrapperIoContext.run();
                    }));
            }
        }

        void    stop()
        {
            std::lock_guard<std::mutex> lck(mIoThreadGuard);

            mWrapperIoContext.stop();
            for (auto& thread : mIoThreads)
            {
                try
                {
                    thread.join();
                }
                catch (...)
                {
                }
            }
            mIoThreads.clear();
        }

        asio::io_context& context()
        {
            return mWrapperIoContext.context();
        }

        WrapperIoContext& wrapperIoContext()
        {
            return mWrapperIoContext;
        }

    private:
        WrapperIoContext            mWrapperIoContext;
        std::vector<std::thread>    mIoThreads;
        std::mutex                  mIoThreadGuard;
    };

    class IoContextPool : public asio::noncopyable
    {
    public:
        using Ptr = std::shared_ptr<IoContextPool>;

        IoContextPool(size_t poolSize,
            int concurrencyHint)
            :
            mPickIoContextIndex(0)
        {
            if (poolSize == 0)
            {
                throw std::runtime_error("pool size is zero");
            }

            for (size_t i = 0; i < poolSize; i++)
            {
                mIoContextPool.emplace_back(std::make_shared<IoContextThread>(concurrencyHint));
            }
        }

        virtual ~IoContextPool()
        {
            stop();
        }

        void  start(size_t threadNumEveryContext)
        {
            std::lock_guard<std::mutex> lck(mPoolGuard);

            for (const auto& context : mIoContextPool)
            {
                context->start(threadNumEveryContext);
            }
        }

        void  stop()
        {
            std::lock_guard<std::mutex> lck(mPoolGuard);

            for (const auto& context : mIoContextPool)
            {
                context->stop();
            }
            mIoContextPool.clear();
        }

        asio::io_context& pickIoContext()
        {
            auto index = mPickIoContextIndex.fetch_add(1, std::memory_order::memory_order_relaxed);
            return mIoContextPool[index % mIoContextPool.size()]->context();
        }

        std::shared_ptr<IoContextThread> pickIoContextThread()
        {
            auto index = mPickIoContextIndex.fetch_add(1, std::memory_order::memory_order_relaxed);
            return mIoContextPool[index % mIoContextPool.size()];
        }

    private:
        std::vector<std::shared_ptr<IoContextThread>>   mIoContextPool;
        std::mutex                                      mPoolGuard;
        std::atomic_int32_t                             mPickIoContextIndex;
    };

    class AsioTcpConnector : public asio::noncopyable
    {
    public:
        using Ptr = std::shared_ptr<AsioTcpConnector>;

        AsioTcpConnector(IoContextPool::Ptr ioContextPool)
            :
            mIoContextPool(ioContextPool)
        {
        }

        void    asyncConnect(
            asio::ip::tcp::endpoint endpoint,
            std::function<void(SharedSocket::Ptr socket)> callback)
        {
            wrapperAsyncConnect(mIoContextPool->pickIoContextThread(), { endpoint }, callback);
        }

        void    asyncConnect(
            std::shared_ptr<IoContextThread> ioContextThread,
            asio::ip::tcp::endpoint endpoint,
            std::function<void(SharedSocket::Ptr socket)> callback)
        {
            wrapperAsyncConnect(ioContextThread, { endpoint }, callback);
        }

    private:
        void    wrapperAsyncConnect(
            IoContextThread::Ptr ioContextThread,
            std::vector<asio::ip::tcp::endpoint> endpoints,
            std::function<void(SharedSocket::Ptr socket)> callback)
        {
            auto sharedSocket = SharedSocket::Make(tcp::socket(ioContextThread->context()), ioContextThread->context());
            asio::async_connect(sharedSocket->socket(),
                endpoints,
                [=](std::error_code ec, tcp::endpoint) {
                    if (!ec)
                    {
                        callback(sharedSocket);
                    }
                });
        }

    private:
        IoContextPool::Ptr mIoContextPool;
    };

    class AsioTcpAcceptor : public asio::noncopyable, public std::enable_shared_from_this<AsioTcpAcceptor>
    {
    public:
        using Ptr = std::shared_ptr< AsioTcpAcceptor>;

        AsioTcpAcceptor(
            asio::io_context& listenContext,
            IoContextPool::Ptr ioContextPool,
            ip::tcp::endpoint endpoint)
            :
            mIoContextPool(ioContextPool),
            mAcceptor(std::make_shared< ip::tcp::acceptor>(listenContext, endpoint))
        {
        }

        virtual ~AsioTcpAcceptor()
        {
            mAcceptor->close();
        }

        void    startAccept(std::function<void(SharedSocket::Ptr)> callback)
        {
            doAccept(callback);
        }

    private:
        void    doAccept(std::function<void(SharedSocket::Ptr)> callback)
        {
            auto& ioContext = mIoContextPool->pickIoContext();
            auto sharedSocket = SharedSocket::Make(tcp::socket(ioContext), ioContext);

            auto self = shared_from_this();
            mAcceptor->async_accept(
                sharedSocket->socket(),
                [self, callback, sharedSocket, this](std::error_code ec) {
                    if (!ec)
                    {
                        sharedSocket->context().post([=]() {
                                callback(sharedSocket);
                            });
                    }
                    doAccept(callback);
                });
        }

    private:
        IoContextPool::Ptr                  mIoContextPool;
        std::shared_ptr< ip::tcp::acceptor> mAcceptor;
    };

    class AsioTcpSession : public asio::noncopyable, public std::enable_shared_from_this< AsioTcpSession>
    {
    public:
        using DataCB = std::function<size_t(const char*, size_t)>;
        using Ptr = std::shared_ptr<AsioTcpSession>;

        static Ptr Make(
            SharedSocket::Ptr socket,
            size_t maxRecvBufferSize,
            DataCB cb)
        {
            struct make_shared_enabler : public AsioTcpSession
            {
            public:
                make_shared_enabler(
                    SharedSocket::Ptr socket,
                    size_t maxRecvBufferSize,
                    DataCB cb)
                    :
                    AsioTcpSession(std::move(socket), maxRecvBufferSize, std::move(cb))
                {}
            };
            auto session = std::make_shared<make_shared_enabler>(
                std::move(socket), 
                maxRecvBufferSize, 
                std::move(cb));
            session->startRecv();
            return session;
        }

        virtual ~AsioTcpSession() = default;

        void    send(std::shared_ptr<std::string> msg)
        {
            {
                std::lock_guard<std::mutex> lck(mSendGuard);
                mPendingSendMsg.push_back({ 0, std::move(msg) });
            }
            trySend();
        }

        void    send(std::string msg)
        {
            send(std::make_shared<std::string>(std::move(msg)));
        }

        const SharedSocket::Ptr& socket() const
        {
            return mSocket;
        }

    private:
        AsioTcpSession(
            SharedSocket::Ptr socket,
            size_t maxRecvBufferSize,
            DataCB cb)
            :
            mMaxRecvBufferSize(maxRecvBufferSize),
            mSocket(std::move(socket)),
            mSending(false),
            mDataCB(std::move(cb))
        {
            mSocket->socket().non_blocking();
            asio::ip::tcp::no_delay option(true);
            mSocket->socket().set_option(option);
            growRecvBuffer();
        }

        void    startRecv()
        {
            std::call_once(mRecvInitOnceFlag, [=]() {
                    doRecv();
                });
        }

        void    doRecv()
        {
            auto self = shared_from_this();
            mSocket->socket().async_read_some(
                asio::buffer(ox_buffer_getwriteptr(mRecvBuffer.get()),
                    ox_buffer_getwritevalidcount(mRecvBuffer.get())),
                [this, self](std::error_code ec, size_t bytesTransferred) {
                    onRecvCompleted(ec, bytesTransferred);
                });
        }

        void    onRecvCompleted(std::error_code ec, size_t bytesTransferred)
        {
            if (ec)
            {
                return;
            }

            ox_buffer_addwritepos(mRecvBuffer.get(), bytesTransferred);
            if (ox_buffer_getreadvalidcount(mRecvBuffer.get()) == ox_buffer_getsize(mRecvBuffer.get()))
            {
                growRecvBuffer();
            }

            if (mDataCB)
            {
                const auto proclen = mDataCB(ox_buffer_getreadptr(mRecvBuffer.get()),
                    ox_buffer_getreadvalidcount(mRecvBuffer.get()));
                assert(proclen <= ox_buffer_getreadvalidcount(mRecvBuffer.get()));
                if (proclen <= ox_buffer_getreadvalidcount(mRecvBuffer.get()))
                {
                    ox_buffer_addreadpos(mRecvBuffer.get(), proclen);
                }
                else
                {
                    ;//throw
                }
            }

            if (ox_buffer_getwritevalidcount(mRecvBuffer.get()) == 0 
                || ox_buffer_getreadvalidcount(mRecvBuffer.get()) == 0)
            {
                ox_buffer_adjustto_head(mRecvBuffer.get());
            }

            doRecv();
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

            auto self = shared_from_this();
            mSocket->socket().async_send(
                mBuffers,
                [this, self](std::error_code ec, size_t bytesTransferred) {
                    onSendCompleted(ec, bytesTransferred);
                });
            mSending = true;
        }

        void    onSendCompleted(std::error_code ec, size_t bytesTransferred)
        {
            {
                std::lock_guard<std::mutex> lck(mSendGuard);
                mSending = false;
                if (ec) //TODO:: 错误处理
                {
                    return;
                }
                adjustSendBuffer(bytesTransferred);
            }
            trySend();
        }

        void    adjustSendBuffer(size_t bytesTransferred)
        {
            while (bytesTransferred > 0)
            {
                auto& frontMsg = mPendingSendMsg.front();
                const auto len = std::min<size_t>(bytesTransferred, frontMsg.msg->size() - frontMsg.sendPos);
                frontMsg.sendPos += len;
                bytesTransferred -= len;
                if (frontMsg.sendPos == frontMsg.msg->size())
                {
                    mPendingSendMsg.pop_front();
                }
            }
        }

        void    growRecvBuffer()
        {
            if (mRecvBuffer == nullptr)
            {
                mRecvBuffer.reset(ox_buffer_new(std::min<size_t>(16 * 1024, mMaxRecvBufferSize)));
            }
            else
            {
                const auto NewSize = ox_buffer_getsize(mRecvBuffer.get()) + 1024;
                if (NewSize > mMaxRecvBufferSize)
                {
                    return;
                }
                std::unique_ptr<struct buffer_s, BufferDeleter> newBuffer(ox_buffer_new(NewSize));
                ox_buffer_write(newBuffer.get(),
                    ox_buffer_getreadptr(mRecvBuffer.get()),
                    ox_buffer_getreadvalidcount(mRecvBuffer.get()));
                mRecvBuffer = std::move(newBuffer);
            }
        }

    private:
        const size_t                        mMaxRecvBufferSize;
        const SharedSocket::Ptr             mSocket;

        bool                                mSending;
        std::mutex                          mSendGuard;
        struct PendingMsg
        {
            size_t  sendPos;
            std::shared_ptr<std::string>    msg;
        };
        // TODO::暂时不使用双缓冲队列,因为它需要用asio::async_write来配合,此函数对性能反而有轻微降低.
        std::deque<PendingMsg>              mPendingSendMsg;
        std::vector<asio::const_buffer>     mBuffers;

        std::once_flag                      mRecvInitOnceFlag;
        DataCB                              mDataCB;
        struct BufferDeleter
        {
            void operator()(struct buffer_s* ptr) const
            {
                ox_buffer_delete(ptr);
            }
        };
        std::unique_ptr<struct buffer_s, BufferDeleter> mRecvBuffer;
    };

}