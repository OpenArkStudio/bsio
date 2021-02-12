#include <asio/signal_set.hpp>
#include <bsio/net/http/HttpFormat.hpp>
#include <bsio/net/wrapper/HttpAcceptorBuilder.hpp>
#include <thread>

using namespace asio;
using namespace asio::ip;
using namespace bsio;
using namespace bsio::net;

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        fprintf(stderr,
                "Usage: <port> "
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

    TcpAcceptor::Ptr acceptor = TcpAcceptor::Make(
            listenContextWrapper.context(),
            ioContextThreadPool,
            ip::tcp::endpoint(ip::tcp::v4(), std::atoi(argv[1])));

    wrapper::HttpAcceptorBuilder builder;
    builder.WithAcceptor(acceptor)
            .WithRecvBufferSize(1024)
            .WithHttpSessionBuilder([](wrapper::HttpSessionBuilder &builder) {
                // here, you can initialize your session user data
                builder.WithEnterCallback([](const bsio::net::http::HttpSession::Ptr &) {
                       })
                        .WithParserCallback([](const bsio::net::http::HTTPParser &parser, const bsio::net::http::HttpSession::Ptr &session) {
                            // we can call parser.getPath() get the query path

                            bsio::net::http::HttpResponse resp;
                            resp.setBody("hello world");
                            if (parser.isKeepAlive())
                            {
                                resp.addHeadValue("Connection", "Keep-Alive");
                                session->send(resp.getResult());
                            }
                            else
                            {
                                resp.addHeadValue("Connection", "Close");
                                session->send(resp.getResult(), [session]() {
                                    session->shutdown(asio::ip::tcp::socket::shutdown_type::shutdown_both);
                                });
                            }
                        });
            })
            .start();

    asio::signal_set sig(listenContextWrapper.context(), SIGINT, SIGTERM);
    sig.async_wait([&](const asio::error_code &err, int signal) {
        stoped = true;
    });

    for (; !stoped;)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    listenContextWrapper.stop();
    ioContextThreadPool->stop();

    return 0;
}
