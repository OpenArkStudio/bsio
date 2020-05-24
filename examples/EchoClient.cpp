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
                .AddSocketProcessingHandler([](asio::ip::tcp::socket& socket)
                {
                    socket.non_blocking(true);
                })
                .WithEstablishHandler([=](asio::ip::tcp::socket socket)
                {
                    auto dataHandler = [=](const TcpSession::Ptr& session, const char* buffer, size_t len) {
                        auto leftLen = len;
                        while(leftLen >= packetSize)
                        {
                            session->send(std::make_shared<std::string>(buffer, packetSize));
                            leftLen -= packetSize;
                            buffer += packetSize;
                        }
                        return len-leftLen;
                    };
                    auto session = bsio::TcpSession::Make(std::move(socket), 1024 * 1024, dataHandler, nullptr);
                    {
                        std::lock_guard<std::mutex> guard(g);
                        sessions.push_back(session);
                    }

                    std::string str(packetSize, 'c');
                    for (size_t i = 0; i < pipelinePacketNum; i++)
                    {
                        session->send(std::make_shared<std::string>(str));
                    }
                })
                .asyncConnect();
        }

        if(false)
        {
            auto dataHandler = [=](const TcpSession::Ptr& session, const char* buffer, size_t len) {
                session->send(std::make_shared<std::string>(buffer, len));
                return len;
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
                .AddEnterCallback([=](const TcpSession::Ptr& session)
                {   
                    g.lock();
                    sessions.push_back(session);
                    g.unlock();
                    
                    std::string str(packetSize, 'c');
                    for (size_t i = 0; i < pipelinePacketNum; i++)
                    {
                        session->send(std::make_shared<std::string>(str));
                    }
                })
                .WithRecvBufferSize(1024)
                .WithClosedHandler([](const TcpSession::Ptr& session)
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
