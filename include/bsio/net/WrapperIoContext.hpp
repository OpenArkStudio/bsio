#pragma once

#include <asio.hpp>
#include <functional>
#include <memory>

namespace bsio::net {

class IoContextThread;

class WrapperIoContext : private asio::noncopyable
{
public:
    using Ptr = std::shared_ptr<WrapperIoContext>;

    explicit WrapperIoContext(int concurrencyHint)
        : mTrickyIoContext(std::make_shared<asio::io_context>(concurrencyHint)),
          mIoContext(*mTrickyIoContext)
    {
    }

    explicit WrapperIoContext(asio::io_context& ioContext)
        : mIoContext(ioContext)
    {
    }

    virtual ~WrapperIoContext()
    {
        stop();
    }

    void run() const
    {
        asio::io_service::work worker(mIoContext);
        while (!mIoContext.stopped())
        {
            mIoContext.run();
        }
    }

    void stop() const
    {
        mIoContext.stop();
    }

    asio::io_context& context() const
    {
        return mIoContext;
    }

    static std::shared_ptr<asio::steady_timer> RunAfter(asio::io_context& context, std::chrono::nanoseconds timeout, std::function<void(void)> callback)
    {
        auto timer = std::make_shared<asio::steady_timer>(context);
        timer->expires_from_now(timeout);
        timer->async_wait([callback = std::move(callback), timer](const asio::error_code& ec) {
            if (!ec)
            {
                callback();
            }
        });
        return timer;
    }

    auto runAfter(std::chrono::nanoseconds timeout, std::function<void(void)> callback) const
    {
        return RunAfter(mIoContext, timeout, std::move(callback));
    }

private:
    std::shared_ptr<asio::io_context> mTrickyIoContext;
    asio::io_context& mIoContext;

    friend IoContextThread;
};

}// namespace bsio::net
