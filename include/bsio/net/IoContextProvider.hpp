#pragma once

#include <asio.hpp>
#include <memory>

namespace bsio::net {

class IoContextProvider
{
public:
    using Ptr = std::shared_ptr<IoContextProvider>;

    virtual ~IoContextProvider() = default;

    virtual asio::io_context& pickIoContext() = 0;
};

}// namespace bsio::net
