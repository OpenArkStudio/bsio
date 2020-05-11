#include <iostream>
#include <bsio/Bsio.hpp>
#include <bsio/wrapper/ConnectorBuilder.hpp>

using namespace bsio;
std::vector <TcpSession::Ptr > sessions;
std::mutex g;

int main(int argc, char** argv)
{
    if (argc != 9)
    {
        fprintf(stderr, "Usage: <host> <port> <client num> "
            " <thread pool size> <concurrencyHint> <thread num one context>"
            " <pipeline packet num> <packet size> \n");
        exit(-1);
    }

    IoContextThreadPool::Ptr ioContextPool = IoContextThreadPool::Make(
        std::atoi(argv[4]), std::atoi(argv[5]));
    ioContextPool->start(std::atoi(argv[6]));

    const auto endpoint = asio::ip::tcp::endpoint(
        asio::ip::address_v4::from_string(argv[1]), std::atoi(argv[2]));
    TcpConnector::Ptr connector = std::make_shared<TcpConnector>(ioContextPool);

    auto pipelinePacketNum = std::atoi(argv[7]);
    auto packetSize = std::atoi(argv[8]);

    for (size_t i = 0; i < std::atoi(argv[3]); i++)
    {
        {
            SocketConnectBuilder connectionBuilder;
            connectionBuilder.WithConnector(connector.get())
                .WithEndpoint(endpoint)
                .WithTimeout(std::chrono::seconds(10))
                .WithFailedHandler([]()
                {
                    std::cout << "connect failed" << std::endl;
                })
                .WithEstablishHandler([=](asio::ip::tcp::socket socket)
                {
                    auto c = [=](TcpSession::Ptr session, const char* buffer, size_t len) {
                        size_t leftLen = len;
                        while (leftLen >= packetSize)
                        {
                            session->send(std::string(buffer, packetSize));
                            leftLen -= packetSize;
                        }
                        return len - leftLen;
                    };
                    auto session = bsio::TcpSession::Make(std::move(socket), 1024 * 1024, c, nullptr);
                    g.lock();
                    sessions.push_back(session);
                    g.unlock();

                    std::string str(packetSize, 'c');
                    for (size_t i = 0; i < pipelinePacketNum; i++)
                    {
                        session->send(str);
                    }
                })
                .asyncConnect();
        }
        {
            auto dataHandler = [=](TcpSession::Ptr session, const char* buffer, size_t len) {
                size_t leftLen = len;
                while (leftLen >= packetSize)
                {
                    session->send(std::string(buffer, packetSize));
                    leftLen -= packetSize;
                }
                return len - leftLen;
            };

            TcpSessionConnectBuilder connectionBuilder;
            connectionBuilder.WithConnector(connector.get())
                .WithEndpoint(endpoint)
                .WithTimeout(std::chrono::seconds(10))
                .WithFailedHandler([]()
                {
                    std::cout << "connect failed" << std::endl;
                })
                .WithDataHandler(dataHandler)
                .AddEnterCallback([=](TcpSession::Ptr session)
                {   
                    g.lock();
                    sessions.push_back(session);
                    g.unlock();
                    
                    std::string str(packetSize, 'c');
                    for (size_t i = 0; i < pipelinePacketNum; i++)
                    {
                        session->send(str);
                    }
                })
                .WithRecvBufferSize(1024)
                .WithClosedHandler([](TcpSession::Ptr session)
                {
                    
                })
                .asyncConnect();
        }
    }

    for (;;)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        g.lock();
        std::cout << "num:" << sessions.size() << std::endl;
        g.unlock();
    }

    return 0;
}