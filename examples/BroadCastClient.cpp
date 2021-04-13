#include <atomic>
#include <bsio/Bsio.hpp>
#include <bsio/base/Packet.hpp>
#include <bsio/net/wrapper/ConnectorBuilder.hpp>
#include <chrono>
#include <iostream>

using namespace bsio;
using namespace bsio::net;
using namespace bsio::base;
using namespace std;

atomic_llong TotalRecvPacketNum = ATOMIC_VAR_INIT(0);
atomic_llong TotalRecvPacketSize = ATOMIC_VAR_INIT(0);
std::atomic_int SessionNum = ATOMIC_VAR_INIT(0);
std::atomic_llong SendingNum = ATOMIC_VAR_INIT(0);

int main(int argc, char** argv)
{
    if (argc != 8)
    {
        fprintf(stderr,
                "Usage:"
                " <host>"
                " <port>"
                " <client num>"
                " <thread pool size>"
                " <concurrencyHint>"
                " <thread num one context>"
                " <packet size>\n");
        exit(-1);
    }

    IoContextThreadPool::Ptr ioContextPool = IoContextThreadPool::Make(
            std::atoi(argv[4]), std::atoi(argv[5]));
    ioContextPool->start(std::atoi(argv[6]));

    const auto endpoint = asio::ip::tcp::endpoint(
            asio::ip::address_v4::from_string(argv[1]), std::atoi(argv[2]));

    const auto packetSize = std::atoi(argv[7]);

    for (size_t i = 0; i < std::atoi(argv[3]); i++)
    {
        auto handler = [=](const TcpSession::Ptr& session, bsio::base::BasePacketReader& reader) {
            while (true)
            {
                const auto buffer = reader.currentBuffer();

                if (!reader.enough(sizeof(uint32_t)))
                {
                    break;
                }
                const auto magicNum = reader.readUINT32();
                if (magicNum != 0x12345678)
                {
                    throw std::runtime_error("magic num error");
                }

                if (!reader.enough(sizeof(uint32_t)))
                {
                    break;
                }

                const auto packetLen = reader.readUINT32();
                if (!reader.enough(packetLen - sizeof(uint32_t)))
                {
                    break;
                }

                TotalRecvPacketSize.fetch_add(packetLen);
                TotalRecvPacketNum.fetch_add(1);

                reader.readUINT16();
                const int64_t addr = reader.readINT64();

                if (addr == reinterpret_cast<int64_t>(session.get()))
                {
                    SendingNum.fetch_add(1);
                    session->send(std::string(buffer, packetLen + sizeof(uint32_t)), []() {
                        SendingNum.fetch_sub(1);
                    });
                }

                reader.addPos(packetLen - sizeof(uint32_t) - sizeof(uint16_t) - sizeof(int64_t));
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
                .WithRecvBufferSize(1024)
                .AddEstablishHandler([=](const TcpSession::Ptr& session) {
                    SessionNum.fetch_add(1);

                    std::cout << "connect success" << std::endl;
                    const auto HeadLen = sizeof(uint32_t) + sizeof(uint16_t);

                    std::shared_ptr<AutoMallocPacket<1024>> sp = std::make_shared<AutoMallocPacket<1024>>(false, true);
                    sp->writeUINT32(0x12345678);
                    sp->writeUINT32(HeadLen + sizeof(int64_t) + packetSize);
                    sp->writeUINT16(1);
                    sp->writeINT64(reinterpret_cast<int64_t>(session.get()));
                    sp->writeBinary(std::string(packetSize, '_'));

                    for (size_t i = 0; i < 1; ++i)
                    {
                        SendingNum.fetch_add(1);
                        session->send(std::string(sp->getData(), sp->getPos()), []() {
                            SendingNum.fetch_sub(1);
                        });
                    }
                })
                .WithDataHandler(handler)
                .WithClosedHandler([](const TcpSession::Ptr& session) {
                    SessionNum.fetch_sub(1);
                })
                .AddSocketProcessingHandler([](asio::ip::tcp::socket& socket) {
                    //asio::socket_base::send_buffer_size sdBufSizeOption(16*1024);
                    //asio::socket_base::receive_buffer_size rdBufSizeOption(16*1024);
                    //socket.set_option(sdBufSizeOption);
                    //socket.set_option(rdBufSizeOption);
                })
                .asyncConnect();
    }

    WrapperIoContext mainLoop(1);

    asio::signal_set sig(mainLoop.context(), SIGINT, SIGTERM);
    sig.async_wait([&](const asio::error_code& err, int signal) {
        mainLoop.stop();
    });

    for (; !mainLoop.context().stopped();)
    {
        mainLoop.context().run_one_for(std::chrono::seconds(1));
        std::cout << "connection num:" << SessionNum.load() << ", packet sending num:" << SendingNum.load() << std::endl;
    }

    ioContextPool->stop();
    mainLoop.stop();

    return 0;
}
