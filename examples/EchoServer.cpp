#include <asio/signal_set.hpp>
#include <thread>
#include <iostream>
#include <bsio/bsio.h>

using namespace bsio;

std::atomic< int64_t> count = 0;

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: <listen port>\n");
        exit(-1);
    }

    bool stoped = false;
    IoContextPool::Ptr ioContextPool = std::make_shared<IoContextPool>(2, 1);
    ioContextPool->start(1);

    IoContextThread listenContextWrapper(1);
    listenContextWrapper.start(1);

    AsioTcpAcceptor::Ptr acceptor = std::make_shared<AsioTcpAcceptor>(listenContextWrapper.context(),
        ioContextPool,
        ip::tcp::endpoint(ip::tcp::v4(), std::atoi(argv[1])));
    acceptor->startAccept([](SharedSocket::Ptr socket) {
            auto session = AsioTcpSession::Make(socket, 1024 * 1024, [](AsioTcpSession::Ptr session, const char* buffer, size_t len) {
                session->send(std::string(buffer, len));
                count++;
                return len;
            });
        });

    asio::signal_set sig(listenContextWrapper.context(), SIGINT, SIGTERM);
    sig.async_wait([&](const asio::error_code & err, int signal)
        {
            stoped = true;
        }
    );

    for (; !stoped;)
    {
        auto nowTime = std::chrono::system_clock::now();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto diff = std::chrono::system_clock::now() - nowTime;
        auto mill = std::chrono::duration_cast<std::chrono::milliseconds>(diff);
        std::cout << count << " cost :" << mill.count() << std::endl;
        count = 0;
    }

    listenContextWrapper.stop();
    ioContextPool->stop();

    return 0;
}