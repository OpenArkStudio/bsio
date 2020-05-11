#include <asio/signal_set.hpp>
#include <thread>
#include <iostream>
#include <bsio/wrapper/HttpAcceptorBuilder.hpp>

using namespace asio;
using namespace asio::ip;
using namespace bsio;

std::atomic< int64_t> count;

int main(int argc, char** argv)
{
    if (argc != 5)
    {
        fprintf(stderr, "Usage: <port> "
            " <thread pool size> <concurrencyHint>"
            " <thread num one context>\n");
        exit(-1);
    }

    bool stoped = false;
    auto ioContextThreadPool = IoContextThreadPool::Make(
        std::atoi(argv[2]), std::atoi(argv[3]));
    ioContextThreadPool->start(std::atoi(argv[4]));

    IoContextThread listenContextWrapper(1);
    listenContextWrapper.start(1);

    TcpAcceptor::Ptr acceptor = std::make_shared<TcpAcceptor>(
        listenContextWrapper.context(),
        ioContextThreadPool,
        ip::tcp::endpoint(ip::tcp::v4(), std::atoi(argv[1])));


    HttpAcceptorBuilder b;
    b.WithAcceptor(acceptor)
    .WithRecvBufferSize(1024)
    .AddEnterCallback([](TcpSession::Ptr)
    {
    })
    .WithClosedHandler([](TcpSession::Ptr)
    {
    })
    .WithEnterCallback([](const bsio::net::http::HttpSession::Ptr&)
    {
            std::cout << "http enter" << std::endl;
    })
    .WithParserCallback([](const bsio::net::http::HTTPParser&, const  bsio::net::http::HttpSession::Ptr&)
    {
            std::cout << "http request" << std::endl;
    })
    .start();

    asio::signal_set sig(listenContextWrapper.context(), SIGINT, SIGTERM);
    sig.async_wait([&](const asio::error_code & err, int signal)
        {
            stoped = true;
        }
    );

    count = 0;
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
    ioContextThreadPool->stop();

    return 0;
}