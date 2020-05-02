#pragma once

#include <memory>
#include <functional>

#include <asio.hpp>

namespace bsio {

    class IoContextThread;

    class WrapperIoContext : public asio::noncopyable
    {
    public:
        using Ptr = std::shared_ptr<WrapperIoContext>;

        virtual ~WrapperIoContext()
        {
            stop();
        }

        void    run() const
        {
            asio::io_service::work worker(mIoContext);
            while (!mIoContext.stopped())
            {
                mIoContext.run();
            }
        }

        void    stop() const
        {
            mIoContext.stop();
        }

        asio::io_context& context() const
        {
            return mIoContext;
        }

        auto    runAfter(std::chrono::nanoseconds timeout, std::function<void(void)> callback) const
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
        explicit WrapperIoContext(int concurrencyHint)
            :
            mTrickyIoContext(std::make_shared<asio::io_context>(concurrencyHint)),
            mIoContext(*mTrickyIoContext)
        {
        }

        explicit WrapperIoContext(asio::io_context& ioContext)
            :
            mIoContext(ioContext)
        {}

    private:
        std::shared_ptr<asio::io_context>   mTrickyIoContext;
        asio::io_context&                   mIoContext;

        friend IoContextThread;
    };

}