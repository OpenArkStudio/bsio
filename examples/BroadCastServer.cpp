#include <asio/signal_set.hpp>
#include <bsio/wrapper/AcceptorBuilder.hpp>
#include <iostream>

using namespace asio;
using namespace asio::ip;
using namespace bsio;
using namespace bsio::net;

std::vector<TcpSession::Ptr> clients;

std::atomic_llong TotalSendLen = ATOMIC_VAR_INIT(0);
std::atomic_llong TotalRecvLen = ATOMIC_VAR_INIT(0);

std::atomic_llong SendPacketNum = ATOMIC_VAR_INIT(0);
std::atomic_llong RecvPacketNum = ATOMIC_VAR_INIT(0);

std::atomic_llong SendingNum = ATOMIC_VAR_INIT(0);

static void addClientID(const TcpSession::Ptr &session)
{
    clients.push_back(session);
}

static void removeClientID(const TcpSession::Ptr &session)
{
    for (auto it = clients.begin(); it != clients.end(); ++it)
    {
        if (*it == session)
        {
            clients.erase(it);
            break;
        }
    }
}

static size_t getClientNum()
{
    return clients.size();
}

static void broadCastPacket(const bsio::net::SendableMsg::Ptr &packet)
{
    const auto packetLen = packet->size();
    RecvPacketNum.fetch_add(1);
    TotalRecvLen += packetLen;

    for (const auto &session : clients)
    {
        SendingNum.fetch_add(1);
        session->send(packet, []() {
            SendingNum.fetch_sub(1);
        });
    }

    SendPacketNum += clients.size();
    TotalSendLen += (clients.size() * packetLen);
}

int main(int argc, char **argv)
{
    asio::io_context mainLoop(1);
    asio::io_service::work worker(mainLoop);

    if (argc != 5)
    {
        fprintf(stderr,
                "Usage:"
                " <port>"
                " <thread pool size>"
                " <concurrencyHint>"
                " <thread num one context>\n");
        exit(-1);
    }

    auto ioContextThreadPool = IoContextThreadPool::Make(
            std::atoi(argv[2]), std::atoi(argv[3]));
    ioContextThreadPool->start(std::atoi(argv[4]));

    IoContextThread listenContextWrapper(1);
    listenContextWrapper.start(1);

    TcpAcceptor::Ptr acceptor = TcpAcceptor::Make(
            listenContextWrapper.context(),
            ioContextThreadPool,
            ip::tcp::endpoint(ip::tcp::v4(), std::atoi(argv[1])));

    wrapper::TcpSessionAcceptorBuilder builder;
    builder.WithAcceptor(acceptor)
            .AddSocketProcessingHandler([](asio::ip::tcp::socket &socket) {
                //asio::socket_base::send_buffer_size sdBufSizeOption(16*1024);
                //asio::socket_base::receive_buffer_size rdBufSizeOption(16*1024);
                //socket.set_option(sdBufSizeOption);
                //socket.set_option(rdBufSizeOption);
            })
            .WithSessionOptionBuilder([=, &mainLoop](wrapper::SessionOptionBuilder &builder) {
                // here, you can initialize your session user data
                auto handler = [=, &mainLoop](const TcpSession::Ptr &session, bsio::base::BasePacketReader &reader) {
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

                        auto packet = bsio::net::MakeStringMsg(buffer, packetLen + sizeof(uint32_t));
                        mainLoop.dispatch([packet]() {
                            broadCastPacket(packet);
                        });

                        reader.addPos(packetLen - sizeof(uint32_t));
                        reader.savePos();
                    }
                };

                builder.WithDataHandler(handler)
                        .WithRecvBufferSize(1024)
                        .AddEnterCallback([&mainLoop](const TcpSession::Ptr &session) {
                            mainLoop.dispatch([session]() {
                                addClientID(session);
                            });
                        })
                        .WithClosedHandler([&mainLoop](const TcpSession::Ptr &session) {
                            std::cout << "connection closed" << std::endl;
                            mainLoop.dispatch([session]() {
                                removeClientID(session);
                            });
                        });
            })
            .start();

    asio::signal_set sig(mainLoop, SIGINT, SIGTERM);
    sig.async_wait([&](const asio::error_code &err, int signal) {
        mainLoop.stop();
    });

    auto last = std::chrono::system_clock::now();
    for (; !mainLoop.stopped();)
    {
        mainLoop.run_one_for(std::chrono::seconds(1));

        const auto now = std::chrono::system_clock::now();
        auto diff = now - last;
        if (diff >= std::chrono::seconds(1))
        {
            const auto msDiff = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
            std::cout << "cost " << msDiff << " ms, client num:" << getClientNum() << ", recv " << (TotalRecvLen / 1024) * 1000 / msDiff << " K/s, "
                      << "num : " << RecvPacketNum * 1000 / msDiff << ", send " << (TotalSendLen / 1024) / 1024 * 1000 / msDiff << " M/s, "
                      << " num: " << SendPacketNum * 1000 / msDiff << ", SendingNum:" << SendingNum.load()
                      << std::endl;
            TotalRecvLen = 0;
            TotalSendLen = 0;
            RecvPacketNum = 0;
            SendPacketNum = 0;
            last = std::chrono::system_clock::now();
        }
    }

    listenContextWrapper.stop();
    ioContextThreadPool->stop();

    return 0;
}
