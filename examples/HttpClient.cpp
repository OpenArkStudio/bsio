#include <iostream>
#include <bsio/Bsio.hpp>
#include <bsio/wrapper/ConnectorBuilder.hpp>
#include <bsio/http/HttpParser.hpp>
#include <bsio/http/HttpService.hpp>
#include <bsio/http/HttpFormat.hpp>
#include <bsio/wrapper/HttpConnectorBuilder.hpp>

using namespace bsio;

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

    bsio::net::http::HttpRequest request;
    request.setMethod(bsio::net::http::HttpRequest::HTTP_METHOD::HTTP_METHOD_GET);
    request.setUrl("/");
    request.addHeadValue("Host", "http://www.httpbin.org/");

    bsio::net::http::HttpQueryParameter p;
    p.add("key", "DCD9C36F1F54A96F707DFBE833600167");
    p.add("appid", "929390");
    p.add("ticket", "140000006FC57764C95D45085373F104"
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

    auto httpMsgHandler = [](const bsio::net::http::HTTPParser&, const bsio::net::http::HttpSession::Ptr&)
    {
        std::cout << "hello" << std::endl;
    };

    HttpConnectionBuilder connectionBuilder;
    connectionBuilder.WithConnector(connector.get())
        .WithEndpoint(endpoint)
        .WithTimeout(std::chrono::seconds(10))
        .WithFailedHandler([]()
            {
                std::cout << "connect failed" << std::endl;
            })
        .AddEnterCallback([=](TcpSession::Ptr session)
                    {
                        session->send(requestStr);
                    })
        .WithEnterCallback([](bsio::net::http::HttpSession::Ptr)
        {
            
        })
        .WithRecvBufferSize(1024)
        .WithClosedHandler([](TcpSession::Ptr session)
                            {

                            })
        .WithParserCallback(httpMsgHandler)
        .asyncConnect();
    std::this_thread::sleep_for(std::chrono::seconds(100));

    return 0;
}