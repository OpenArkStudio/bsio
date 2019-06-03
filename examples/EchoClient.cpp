#include <iostream>
#include <bsio/bsio.h>

using namespace bsio;
std::vector < AsioTcpSession::Ptr > sessions;
std::mutex g;

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: <host> <port> <num>\n");
        exit(-1);
    }

    IoContextPool::Ptr ioContextPool = std::make_shared<IoContextPool>(2, 1);
    ioContextPool->start(1);

    const auto endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::from_string(argv[1]), std::atoi(argv[2]));
    AsioTcpConnector connector(ioContextPool);

    for (size_t i = 0; i < std::atoi(argv[3]); i++)
    {
        connector.asyncConnect(endpoint,
            std::chrono::seconds(10),
            [](SharedSocket::Ptr sharedSocket) {
                auto c = [](
                    AsioTcpSession::Ptr session, const char* buffer, size_t len) {
                        session->send(std::string(buffer, len));
                        return len;
                };
                auto session = bsio::AsioTcpSession::Make(sharedSocket, 1024 * 1024, c);
                g.lock();
                sessions.push_back(session);
                g.unlock();

                session->send(std::string("asegaaweahgewhweswahh"));
            },
            []() {
            });
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