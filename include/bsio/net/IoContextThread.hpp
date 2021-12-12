#pragma once

#include <bsio/net/WrapperIoContext.hpp>
#include <memory>
#include <mutex>

namespace bsio::net {

class IoContextThread : private asio::noncopyable
{
public:
    using Ptr = std::shared_ptr<IoContextThread>;

    explicit IoContextThread(int concurrencyHint)
        : mWrapperIoContext(concurrencyHint)
    {
    }

    explicit IoContextThread(asio::io_context& ioContext)
        : mWrapperIoContext(ioContext)
    {
    }

    virtual ~IoContextThread() noexcept
    {
        stop();
    }

    void start(size_t threadNum)
    {
        if (threadNum == 0)
        {
            throw std::runtime_error("thread num is zero");
        }
        std::lock_guard<std::mutex> lck(mIoThreadGuard);
        if (!mIoThreads.empty())
        {
            return;
        }
        for (size_t i = 0; i < threadNum; i++)
        {
            mIoThreads.emplace_back(std::thread([this]() {
                mWrapperIoContext.run();
            }));
        }
    }

    void stop() noexcept
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

    asio::io_context& context() const
    {
        return mWrapperIoContext.context();
    }

    WrapperIoContext& wrapperIoContext()
    {
        return mWrapperIoContext;
    }

private:
    WrapperIoContext mWrapperIoContext;
    std::vector<std::thread> mIoThreads;
    std::mutex mIoThreadGuard;
};

}// namespace bsio::net
