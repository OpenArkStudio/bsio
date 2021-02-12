#pragma once

#include <bsio/net/wrapper/internal/Option.hpp>

namespace bsio::net::wrapper::internal {

template<typename Derived>
class BaseSessionOptionBuilder
{
public:
    Derived &AddEnterCallback(TcpSessionEstablishHandler handler) noexcept
    {
        mTcpSessionOption.establishHandlers.push_back(std::move(handler));
        return static_cast<Derived &>(*this);
    }

    Derived &WithClosedHandler(TcpSession::ClosedHandler handler) noexcept
    {
        mTcpSessionOption.closedHandler = std::move(handler);
        return static_cast<Derived &>(*this);
    }

    const internal::TcpSessionOption &Option() const
    {
        return mTcpSessionOption;
    }

private:
    internal::TcpSessionOption mTcpSessionOption;
};

class SessionOptionBuilder : public BaseSessionOptionBuilder<SessionOptionBuilder>,
                             public asio::noncopyable
{
};

}// namespace bsio::net::wrapper::internal
