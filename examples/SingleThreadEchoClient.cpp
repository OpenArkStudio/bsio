#include <atomic>
#include <bsio/Bsio.hpp>
#include <bsio/net/wrapper/ConnectorBuilder.hpp>
#include <chrono>
#include <iostream>

using namespace bsio;
using namespace bsio::net;

std::atomic_int sessionNum = {0};

int main(int argc, char** argv)
{
    if (argc != 6)
    {
        fprintf(stderr,
                "Usage: <host> <port> <client num> "
                " <pipeline packet num> <packet size> \n");
        exit(-1);
    }

    WrapperIoContext main(1);
    auto mainIoContextProvider = std::make_shared<FixedIoContextProvider>(main.context());

    const auto endpoint = asio::ip::tcp::endpoint(
            asio::ip::address_v4::from_string(argv[1]), std::atoi(argv[2]));

    auto pipelinePacketNum = std::atoi(argv[4]);
    auto packetSize = std::atoi(argv[5]);

    for (size_t i = 0; i < std::atoi(argv[3]); i++)
    {
        auto handler = [=](const TcpSession::Ptr& session, bsio::base::BasePacketReader& reader) {
            while (reader.enough(packetSize))
            {
                session->send(std::string(reader.currentBuffer(), packetSize));
                reader.addPos(packetSize);
                reader.savePos();
            }
        };

        wrapper::TcpSessionConnectorBuilder connectionBuilder;
        connectionBuilder.WithConnector(TcpConnector(mainIoContextProvider))
                .WithEndpoint(endpoint)
                .WithTimeout(std::chrono::seconds(10))
                .WithFailedHandler([]() {
                    std::cout << "connect failed" << std::endl;
                })
                .AddEstablishHandler([=](const TcpSession::Ptr& session) {
                    sessionNum.fetch_add(1);

                    std::string str(packetSize, 'c');
                    for (size_t i = 0; i < pipelinePacketNum; i++)
                    {
                        session->send(str);
                    }
                })
                .WithRecvBufferSize(1024)
                .WithDataHandler(handler)
                .WithClosedHandler([](const TcpSession::Ptr& session) {
                    sessionNum.fetch_sub(1);
                })
                .asyncConnect();
    }

    main.run();

    return 0;
}
