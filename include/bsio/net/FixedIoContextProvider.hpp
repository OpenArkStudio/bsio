#pragma once

#include <bsio/net/IoContextProvider.hpp>

namespace bsio::net {

class FixedIoContextProvider : public IoContextProvider
{
public:
    FixedIoContextProvider(asio::io_context& context)
        : mContext(context)
    {
    }

    asio::io_context& pickIoContext() override
    {
        return mContext;
    }

private:
    asio::io_context& mContext;
};

}// namespace bsio::net
