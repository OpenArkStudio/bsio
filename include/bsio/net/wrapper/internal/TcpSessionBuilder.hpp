#pragma once

#include <bsio/net/wrapper/internal/Option.hpp>

namespace bsio::net::wrapper::internal {

template<typename Derived>
class BaseSessionOptionBuilder
{
public:
    Derived &AddEstablishHandler(TcpSessionEstablishHandler handler) noexcept
    {
        mTcpSessionOption.establishHandlers.push_back(std::move(handler));
        return static_cast<Derived &>(*this);
    }

    Derived &WithDataHandler(TcpSession::DataHandler handler) noexcept
    {
        mTcpSessionOption.dataHandler = std::move(handler);
        return static_cast<Derived &>(*this);
    }

    Derived &WithClosedHandler(TcpSession::ClosedHandler handler) noexcept
    {
        mTcpSessionOption.closedHandler = std::move(handler);
        return static_cast<Derived &>(*this);
    }

    Derived &WithEofHandler(TcpSession::EofHandler handler) noexcept
    {
        mTcpSessionOption.eofHandler = std::move(handler);
        return static_cast<Derived &>(*this);
    }

    [[nodiscard]] const internal::TcpSessionOption &Option() const
    {
        return mTcpSessionOption;
    }

protected:
    void clear()
    {
        mTcpSessionOption.establishHandlers.clear();
    }

private:
    internal::TcpSessionOption mTcpSessionOption;
};

class SessionOptionBuilder : public BaseSessionOptionBuilder<SessionOptionBuilder>,
                             public asio::noncopyable
{
};

}// namespace bsio::net::wrapper::internal
