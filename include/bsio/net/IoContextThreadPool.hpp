#pragma once

#include <bsio/net/IoContextThread.hpp>
#include <memory>
#include <mutex>

namespace bsio::net {

class IoContextThreadPool : private asio::noncopyable
{
public:
    using Ptr = std::shared_ptr<IoContextThreadPool>;

    static Ptr Make(size_t poolSize, int concurrencyHint)
    {
        return std::make_shared<IoContextThreadPool>(poolSize, concurrencyHint);
    }

    IoContextThreadPool(size_t poolSize, int concurrencyHint)
        : mPickIoContextIndex(0)
    {
        if (poolSize == 0)
        {
            throw std::runtime_error("pool size is zero");
        }

        for (size_t i = 0; i < poolSize; i++)
        {
            mIoContextThreadList.emplace_back(std::make_shared<IoContextThread>(concurrencyHint));
        }
    }

    virtual ~IoContextThreadPool() noexcept
    {
        stop();
    }

    void start(size_t threadNumEveryContext)
    {
        std::lock_guard<std::mutex> lck(mPoolGuard);

        for (const auto& ioContextThread : mIoContextThreadList)
        {
            ioContextThread->start(threadNumEveryContext);
        }
    }

    void stop()
    {
        std::lock_guard<std::mutex> lck(mPoolGuard);

        for (const auto& ioContextThread : mIoContextThreadList)
        {
            ioContextThread->stop();
        }
    }

    asio::io_context& pickIoContext()
    {
        const auto index = mPickIoContextIndex.fetch_add(1, std::memory_order::memory_order_relaxed);
        return mIoContextThreadList[index % mIoContextThreadList.size()]->context();
    }

    std::shared_ptr<IoContextThread> pickIoContextThread()
    {
        const auto index = mPickIoContextIndex.fetch_add(1, std::memory_order::memory_order_relaxed);
        return mIoContextThreadList[index % mIoContextThreadList.size()];
    }

private:
    std::vector<std::shared_ptr<IoContextThread>> mIoContextThreadList;
    std::mutex mPoolGuard;
    std::atomic_long mPickIoContextIndex;
};

}// namespace bsio::net
