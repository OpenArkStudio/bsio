#include <asio/signal_set.hpp>
#include <bsio/net/FixedIoContextProvider.hpp>
#include <bsio/net/WrapperIoContext.hpp>
#include <bsio/net/wrapper/AcceptorBuilder.hpp>
#include <iostream>
#include <thread>

using namespace asio;
using namespace asio::ip;
using namespace bsio;
using namespace bsio::net;

std::atomic<int64_t> count = {0};

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr,
                "Usage: <port> "
                " <packet size> \n");
        exit(-1);
    }

    bool stoped = false;

    auto packetSize = std::atoi(argv[2]);

    WrapperIoContext main(1);
    auto mainIoContextProvider = std::make_shared<FixedIoContextProvider>(main.context());
    TcpAcceptor::Ptr acceptor = TcpAcceptor::Make(
            main.context(),
            mainIoContextProvider,
            ip::tcp::endpoint(ip::tcp::v4(), std::atoi(argv[1])));

    wrapper::TcpSessionAcceptorBuilder builder;
    builder.WithAcceptor(acceptor)
            .WithRecvBufferSize(1024)
            .WithSessionOptionBuilder([=](wrapper::SessionOptionBuilder &builder) {
                // here, you can initialize your session user data
                auto handler = [=](const TcpSession::Ptr &session, bsio::base::BasePacketReader &reader) {
                    while (reader.enough(packetSize))
                    {
                        session->send(std::string(reader.currentBuffer(), packetSize));
                        reader.addPos(packetSize);
                        reader.savePos();
                        ++count;
                    }
                };

                builder.AddEstablishHandler([handler](const TcpSession::Ptr &session) {
                       })
                        .WithDataHandler(handler)
                        .WithClosedHandler([](const TcpSession::Ptr &) {
                        });
            })
            .start();

    asio::signal_set sig(main.context(), SIGINT, SIGTERM);
    sig.async_wait([&](const asio::error_code &err, int signal) {
        stoped = true;
        main.stop();
    });

    std::thread print([&]() {
        for (; !stoped;)
        {
            auto nowTime = std::chrono::system_clock::now();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            auto diff = std::chrono::system_clock::now() - nowTime;
            auto mill = std::chrono::duration_cast<std::chrono::milliseconds>(diff);
            std::cout << count << " cost :" << mill.count() << std::endl;
            count = 0;
        }
    });
    main.run();
    print.join();

    main.stop();

    return 0;
}
