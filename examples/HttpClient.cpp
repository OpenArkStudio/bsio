#include <bsio/Bsio.hpp>
#include <bsio/net/http/HttpFormat.hpp>
#include <bsio/net/http/HttpParser.hpp>
#include <bsio/net/http/HttpService.hpp>
#include <bsio/net/wrapper/HttpConnectorBuilder.hpp>
#include <iostream>

using namespace bsio;
using namespace bsio::net;

int main(int argc, char **argv)
{
    if (argc != 6)
    {
        fprintf(stderr,
                "Usage: <host> <port> "
                "<thread pool size> <concurrencyHint> <thread num one context>\n");
        exit(-1);
    }

    IoContextThreadPool::Ptr ioContextPool = IoContextThreadPool::Make(
            std::atoi(argv[3]), std::atoi(argv[4]));
    ioContextPool->start(std::atoi(argv[5]));

    const auto endpoint = asio::ip::tcp::endpoint(
            asio::ip::address_v4::from_string(argv[1]), std::atoi(argv[2]));

    http::HttpRequest request;
    request.setMethod(http::HttpRequest::HTTP_METHOD::HTTP_METHOD_GET);
    request.setUrl("/");
    request.addHeadValue("Host", "http://www.httpbin.org/");

    http::HttpQueryParameter p;
    p.add("key", "DCD9C36F1F54A96F707DFBE833600167");
    p.add("appid", "929390");
    p.add("ticket",
          "140000006FC57764C95D45085373F104"
          "01001001359F745C1800000001000000020000009"
          "DACD3DE1202A8C0431E100003000000B200000032"
          "000000040000005373F104010010016E2E0E009D"
          "ACD3DE1202A8C000000000AAA16F5C2A518B5C"
          "0100FC96040000000000061129B849B0397DD"
          "62E0B1B0373451EC08E1BAB70FC18E21094F"
          "C5F4674EDD50226ABB33D71C601B8E65542F"
          "B9A9F48BFF87AC30904D272FAD5F15CD2D5428"
          "D44827BA58A45886119D6244D672A0C1909C5D"
          "7BD9096D96EB8BAC30E006BE6D405E5B25659"
          "CF3D343C9627078C5FD4CE0120D80DDB2FA09E76111143F132CA0B");
    request.setQuery(p.getResult());

    std::string requestStr = request.getResult();

    wrapper::HttpConnectorBuilder connectionBuilder;
    connectionBuilder.WithConnector(TcpConnector(ioContextPool))
            .WithEndpoint(endpoint)
            .WithTimeout(std::chrono::seconds(10))
            .WithFailedHandler([]() {
                std::cout << "connect failed" << std::endl;
            })
            .WithEnterCallback([=](const http::HttpSession::Ptr &session) {
                session->send(requestStr.c_str(), requestStr.size());
            })
            .WithRecvBufferSize(1024)
            .WithParserCallback([](const http::HTTPParser &parser, const http::HttpSession::Ptr &) {
                std::cout << "recv resp:" << parser.getBody() << std::endl;
            })
            .asyncConnect();

    std::this_thread::sleep_for(std::chrono::seconds(15));

    return 0;
}
