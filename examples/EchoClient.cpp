#include <atomic>
#include <bsio/Bsio.hpp>
#include <bsio/wrapper/ConnectorBuilder.hpp>
#include <chrono>
#include <iostream>

using namespace bsio;
using namespace bsio::net;

std::atomic_int sessionNum = {0};

int main(int argc, char** argv)
{
    if (argc != 9)
    {
        fprintf(stderr,
                "Usage: <host> <port> <client num> "
                " <thread pool size> <concurrencyHint> <thread num one context>"
                " <pipeline packet num> <packet size> \n");
        exit(-1);
    }

    IoContextThreadPool::Ptr ioContextPool = IoContextThreadPool::Make(
            std::atoi(argv[4]), std::atoi(argv[5]));
    ioContextPool->start(std::atoi(argv[6]));

    const auto endpoint = asio::ip::tcp::endpoint(
            asio::ip::address_v4::from_string(argv[1]), std::atoi(argv[2]));

    auto pipelinePacketNum = std::atoi(argv[7]);
    auto packetSize = std::atoi(argv[8]);

    for (size_t i = 0; i < std::atoi(argv[3]); i++)
    {
        auto handler = [=](const TcpSession::Ptr& session, bsio::base::BasePacketReader reader) {
            while (reader.enough(packetSize))
            {
                session->send(std::string(reader.currentBuffer(), packetSize));
                reader.addPos(packetSize);
                reader.savePos();
            }
        };

        wrapper::TcpSessionConnectorBuilder connectionBuilder;
        connectionBuilder.WithConnector(TcpConnector(ioContextPool))
                .WithEndpoint(endpoint)
                .WithTimeout(std::chrono::seconds(10))
                .WithFailedHandler([]() {
                    std::cout << "connect failed" << std::endl;
                })
                .WithDataHandler(handler)
                .AddEnterCallback([=](const TcpSession::Ptr& session) {
                    sessionNum.fetch_add(1);

                    std::string str(packetSize, 'c');
                    for (size_t i = 0; i < pipelinePacketNum; i++)
                    {
                        session->send(str);
                    }
                })
                .WithRecvBufferSize(1024)
                .WithClosedHandler([](const TcpSession::Ptr& session) {
                    sessionNum.fetch_sub(1);
                })
                .asyncConnect();
    }

    for (;;)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "num:" << sessionNum.load() << std::endl;
    }

    return 0;
}
