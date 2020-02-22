#pragma once

#include <algorithm>
#include <memory>
#include <functional>
#include <mutex>
#include <deque>

#include <asio.hpp>

namespace bsio {

    class AsioTcpAcceptor;
    class AsioTcpConnector;

    class SharedSocket : public asio::noncopyable
    {
    public:
        using Ptr = std::shared_ptr<SharedSocket>;

        virtual ~SharedSocket() = default;

        asio::ip::tcp::socket& socket()
        {
            return mSocket;
        }

        asio::io_context& context()
        {
            return mIoContext;
        }

        SharedSocket(asio::ip::tcp::socket socket, asio::io_context& ioContext)
            :
            mSocket(std::move(socket)),
            mIoContext(ioContext)
        {
        }

    private:
        static SharedSocket::Ptr Make(asio::ip::tcp::socket socket, asio::io_context& ioContext)
        {
            return std::make_shared<SharedSocket>(std::move(socket), ioContext);
        }

    private:
        asio::ip::tcp::socket   mSocket;
        asio::io_context&       mIoContext;

        friend class AsioTcpAcceptor;
        friend class AsioTcpConnector;
    };

    class IoContextThread;

    class WrapperIoContext : public asio::noncopyable
    {
    public:
        using Ptr = std::shared_ptr<WrapperIoContext>;

        virtual ~WrapperIoContext()
        {
            stop();
        }

        void    run()
        {
            asio::io_service::work worker(mIoContext);
            while (!mIoContext.stopped())
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

        auto    runAfter(std::chrono::nanoseconds timeout, std::function<void(void)> callback)
        {
            auto timer = std::make_shared<asio::steady_timer>(mIoContext);
            timer->expires_from_now(timeout);
            timer->async_wait([timer, callback](const asio::error_code & ec) {
                    if (!ec)
                    {
                        callback();
                    }
                });
            return timer;
        }

    private:
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

    private:
        std::shared_ptr<asio::io_context>   mTrickyIoContext;
        asio::io_context&                   mIoContext;

        friend IoContextThread;
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

    class IoContextThreadPool : public asio::noncopyable
    {
    public:
        using Ptr = std::shared_ptr<IoContextThreadPool>;

        static  Ptr Make(size_t poolSize,
            int concurrencyHint)
        {
            return std::make_shared<IoContextThreadPool>(poolSize, concurrencyHint);
        }

        IoContextThreadPool(size_t poolSize,
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
                mIoContextThreadList.emplace_back(
                    std::make_shared<IoContextThread>(concurrencyHint));
            }
        }

        virtual ~IoContextThreadPool()
        {
            stop();
        }

        void  start(size_t threadNumEveryContext)
        {
            std::lock_guard<std::mutex> lck(mPoolGuard);

            for (const auto& ioContextThread : mIoContextThreadList)
            {
                ioContextThread->start(threadNumEveryContext);
            }
        }

        void  stop()
        {
            std::lock_guard<std::mutex> lck(mPoolGuard);

            for (const auto& ioContextThread : mIoContextThreadList)
            {
                ioContextThread->stop();
            }
            mIoContextThreadList.clear();
        }

        asio::io_context& pickIoContext()
        {
            auto index = mPickIoContextIndex.fetch_add(1, std::memory_order::memory_order_relaxed);
            return mIoContextThreadList[index % mIoContextThreadList.size()]->context();
        }

        std::shared_ptr<IoContextThread> pickIoContextThread()
        {
            auto index = mPickIoContextIndex.fetch_add(1, std::memory_order::memory_order_relaxed);
            return mIoContextThreadList[index % mIoContextThreadList.size()];
        }

    private:
        std::vector<std::shared_ptr<IoContextThread>>   mIoContextThreadList;
        std::mutex                                      mPoolGuard;
        std::atomic_long                                mPickIoContextIndex;
    };

    using SocketEstablishHandler = std::function<void(asio::ip::tcp::socket)>;

    class AsioTcpConnector
    {
    public:
        using Ptr = std::shared_ptr<AsioTcpConnector>;

        AsioTcpConnector(IoContextThreadPool::Ptr ioContextThreadPool)
            :
            mIoContextThreadPool(ioContextThreadPool)
        {
        }

        void    asyncConnect(
            asio::ip::tcp::endpoint endpoint,
            std::chrono::nanoseconds timeout,
            SocketEstablishHandler successCallback,
            std::function<void(void)> failedCallback)
        {
            wrapperAsyncConnect(mIoContextThreadPool->pickIoContextThread(), 
                { endpoint }, timeout, successCallback, failedCallback);
        }

        void    asyncConnect(
            std::shared_ptr<IoContextThread> ioContextThread,
            asio::ip::tcp::endpoint endpoint,
            std::chrono::nanoseconds timeout,
            SocketEstablishHandler successCallback,
            std::function<void(void)> failedCallback)
        {
            wrapperAsyncConnect(ioContextThread, { endpoint }, timeout, successCallback, failedCallback);
        }

    private:
        void    wrapperAsyncConnect(
            IoContextThread::Ptr ioContextThread,
            std::vector<asio::ip::tcp::endpoint> endpoints,
            std::chrono::nanoseconds timeout,
            SocketEstablishHandler successCallback,
            std::function<void(void)> failedCallback)
        {
            auto sharedSocket = SharedSocket::Make(
                asio::ip::tcp::socket(ioContextThread->context()), ioContextThread->context());
            auto timeoutTimer = ioContextThread->wrapperIoContext().runAfter(timeout, [=]() {
                    failedCallback();
                });

            asio::async_connect(sharedSocket->socket(),
                endpoints,
                [=](std::error_code ec, asio::ip::tcp::endpoint) {
                    timeoutTimer->cancel();
                    if (!ec)
                    {
                        successCallback(std::move(sharedSocket->socket()));
                    }
                    else
                    {
                        failedCallback();
                    }
                });
        }

    private:
        IoContextThreadPool::Ptr mIoContextThreadPool;
    };

    class AsioTcpAcceptor : public asio::noncopyable, public std::enable_shared_from_this<AsioTcpAcceptor>
    {
    public:
        using Ptr = std::shared_ptr< AsioTcpAcceptor>;

        AsioTcpAcceptor(
            asio::io_context& listenContext,
            IoContextThreadPool::Ptr ioContextThreadPool,
            asio::ip::tcp::endpoint endpoint)
            :
            mIoContextThreadPool(ioContextThreadPool),
            mAcceptor(std::make_shared<asio::ip::tcp::acceptor>(listenContext, endpoint))
        {
        }

        virtual ~AsioTcpAcceptor()
        {
            close();
        }

        void    startAccept(SocketEstablishHandler callback)
        {
            doAccept(callback);
        }
        
        void    close()
        {
            mAcceptor->close();
        }

    private:
        void    doAccept(SocketEstablishHandler callback)
        {
            if (!mAcceptor->is_open())
            {
                return;
            }

            auto& ioContext = mIoContextThreadPool->pickIoContext();
            auto sharedSocket = SharedSocket::Make(asio::ip::tcp::socket(ioContext), ioContext);

            auto self = shared_from_this();
            mAcceptor->async_accept(
                sharedSocket->socket(),
                [self, callback, sharedSocket, this](std::error_code ec) {
                    if (!ec)
                    {
                        sharedSocket->context().post([=]() {
                                callback(std::move(sharedSocket->socket()));
                            });
                    }
                    doAccept(callback);
                });
        }

    private:
        IoContextThreadPool::Ptr                    mIoContextThreadPool;
        std::shared_ptr<asio::ip::tcp::acceptor>    mAcceptor;
    };

    const size_t MinReceivePrepareSize = 1024;

    class AsioTcpSession : public asio::noncopyable, public std::enable_shared_from_this< AsioTcpSession>
    {
    public:
        using Ptr = std::shared_ptr<AsioTcpSession>;
        using DataCB = std::function<size_t(Ptr, const char*, size_t)>;
        using ClosedHandler = std::function<void(Ptr)>;
        using SendCompletedCallback = std::function<void(void)>;

        static Ptr Make(
            asio::ip::tcp::socket socket,
            size_t maxRecvBufferSize,
            DataCB cb,
            ClosedHandler closedHandler)
        {
            struct make_shared_enabler : public AsioTcpSession
            {
            public:
                make_shared_enabler(
                    asio::ip::tcp::socket socket,
                    size_t maxRecvBufferSize,
                    DataCB cb,
                    ClosedHandler closedHandler)
                    :
                    AsioTcpSession(std::move(socket), 
                        maxRecvBufferSize, 
                        std::move(cb),
                        std::move(closedHandler))
                {}
            };

            auto session = std::make_shared<make_shared_enabler>(
                std::move(socket), 
                maxRecvBufferSize, 
                std::move(cb),
                std::move(closedHandler));

            session->startRecv();

            return static_cast<Ptr>(session);
        }

        virtual ~AsioTcpSession() = default;

        void    postClose()
        {
            mSocket
                .get_io_context()
                .post([self = shared_from_this(), this]() {
                    mSocket.close();
                });
        }

        void    postShutdown(asio::ip::tcp::socket::shutdown_type type)
        {
            mSocket
                .get_io_context()
                .post([self = shared_from_this(), this, type]() {
                    mSocket.shutdown(type);
                });
        }

        void    send(std::shared_ptr<std::string> msg, SendCompletedCallback callback = nullptr)
        {
            {
                std::lock_guard<std::mutex> lck(mSendGuard);
                mPendingSendMsg.push_back({ 0, std::move(msg), std::move(callback) });
            }
            trySend();
        }

        void    send(std::string msg, SendCompletedCallback callback = nullptr)
        {
            send(std::make_shared<std::string>(std::move(msg)), std::move(callback));
        }

    private:
        AsioTcpSession(
            asio::ip::tcp::socket socket,
            size_t maxRecvBufferSize,
            DataCB cb,
            ClosedHandler closedHandler)
            :
            mMaxRecvBufferSize(maxRecvBufferSize),
            mSocket(std::move(socket)),
            mSending(false),
            mDataCB(std::move(cb)),
            mReceiveBuffer(std::max<size_t>(MinReceivePrepareSize, maxRecvBufferSize)),
            mClosedHandler(std::move(closedHandler))
        {
            mSocket.non_blocking();
            asio::ip::tcp::no_delay option(true);
            mSocket.set_option(option);
        }

        void    startRecv()
        {
            std::call_once(mRecvInitOnceFlag, [self = shared_from_this(), this]() {
                    doRecv();
                });
        }

        void    doRecv()
        {
            try
            {
                mSocket.async_receive(mReceiveBuffer.prepare(MinReceivePrepareSize),
                    [self = shared_from_this(), this](std::error_code ec, size_t bytesTransferred) {
                    onRecvCompleted(ec, bytesTransferred);
                });
            }
            catch (const std::length_error& ec)
            {
                //TODO::callback to user
            }
        }

        void    onRecvCompleted(std::error_code ec, size_t bytesTransferred)
        {
            if (ec)
            {
                //TODO::处理error code
                return;
            }

            mReceiveBuffer.commit(bytesTransferred);

            if (mDataCB)
            {
                auto validReadBuffer = mReceiveBuffer.data();
                const auto proclen = mDataCB(shared_from_this(), 
                    (const char*)validReadBuffer.data(), 
                    validReadBuffer.size());
                assert(proclen <= validReadBuffer.size());
                if (proclen <= validReadBuffer.size())
                {
                    mReceiveBuffer.consume(proclen);
                }
                else
                {
                    ;//throw
                }
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

            mSocket.async_send(
                mBuffers,
                [self = shared_from_this(), this](std::error_code ec, size_t bytesTransferred) {
                    onSendCompleted(ec, bytesTransferred);
                });
            mSending = true;
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

    private:
        const size_t                        mMaxRecvBufferSize;
        asio::ip::tcp::socket               mSocket;

        // 同时只能发起一次send writev请求
        bool                                mSending;
        std::mutex                          mSendGuard;
        struct PendingMsg
        {
            size_t  sendPos;
            std::shared_ptr<std::string>    msg;
            SendCompletedCallback           callback;
        };
        // TODO::暂时不使用双缓冲队列,因为它需要用asio::async_write来配合,此函数对性能反而有轻微降低.
        std::deque<PendingMsg>              mPendingSendMsg;
        std::vector<asio::const_buffer>     mBuffers;

        std::once_flag                      mRecvInitOnceFlag;
        DataCB                              mDataCB;
        asio::streambuf                     mReceiveBuffer;
        ClosedHandler                       mClosedHandler;
    };

}