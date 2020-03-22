#pragma once

#include <memory>
#include <mutex>

#include <bsio/WrapperIoContext.hpp>

namespace bsio {

    class IoContextThread : public asio::noncopyable
    {
    public:
        using Ptr = std::shared_ptr<IoContextThread>;

        explicit IoContextThread(int concurrencyHint)
            :
            mWrapperIoContext(concurrencyHint)
        {
        }

        explicit IoContextThread(asio::io_context& ioContext)
            :
            mWrapperIoContext(ioContext)
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

}