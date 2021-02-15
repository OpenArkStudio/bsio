#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>

namespace bsio { namespace base {

class WaitGroup
{
public:
    typedef std::shared_ptr<WaitGroup> Ptr;

    static Ptr Create()
    {
        struct make_shared_enabler : public WaitGroup {
        };
        return std::make_shared<make_shared_enabler>();
    }

    WaitGroup(const WaitGroup&) = delete;
    const WaitGroup& operator=(const WaitGroup&) = delete;

public:
    void add(int i = 1)
    {
        mCounter += i;
    }

    void done()
    {
        mCounter--;
        mCond.notify_all();
    }

    void wait()
    {
        std::unique_lock<std::mutex> l(mMutex);
        mCond.wait(l, [&] {
            return mCounter <= 0;
        });
    }

    template<class Rep, class Period>
    void wait(const std::chrono::duration<Rep, Period>& timeout)
    {
        std::unique_lock<std::mutex> l(mMutex);
        mCond.wait_for(l, timeout, [&] {
            return mCounter <= 0;
        });
    }

private:
    WaitGroup()
        : mCounter(0)
    {
    }

    virtual ~WaitGroup() = default;

private:
    std::mutex mMutex;
    std::atomic<int> mCounter;
    std::condition_variable mCond;
};

}}// namespace bsio::base