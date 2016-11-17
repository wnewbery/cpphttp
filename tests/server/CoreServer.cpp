#include <boost/test/unit_test.hpp>
#include "client/Client.hpp"
#include "client/SocketFactory.hpp"
#include "server/CoreServer.hpp"
#include "Response.hpp"
#include "../TestThread.hpp"
#include <iostream>
#include <thread>

using namespace http;

BOOST_AUTO_TEST_SUITE(TestCoreServer)

static const uint16_t BASE_PORT = 5100;

class Server : public http::CoreServer
{
protected:
    virtual http::Response handle_request(http::Request &)override
    {
        http::Response resp;
        resp.status_code(200);
        resp.headers.add("Content-Type", "text/plain");
        resp.body = "OK";
        return resp;
    }
    virtual http::Response parser_error_page(const http::ParserError &)override
    {
        throw std::runtime_error("Unexpected parser_error_page");
    }
};

void success_server_thread(Server *server)
{
    server->run();
}
BOOST_AUTO_TEST_CASE(success)
{
    Server server;
    server.add_tcp_listener("127.0.0.1", BASE_PORT + 0);
    server.add_tls_listener("127.0.0.1", BASE_PORT + 1, "localhost");

    TestThread server_thread(std::bind(&Server::run, &server));

    http::DefaultSocketFactory socket_factory;

    {
        // http://
        http::Request req;
        req.method = GET;
        req.headers.add("Host", "localhost");
        req.raw_url = "/index.html";

        auto resp = http::Client("localhost", BASE_PORT + 0, false, &socket_factory).make_request(req);

        BOOST_CHECK_EQUAL(200, resp.status.code);
        BOOST_CHECK_EQUAL("OK", resp.body);
    }

    {
        // https:// - TLS
        http::Request req;
        req.method = GET;
        req.headers.add("Host", "localhost");
        req.raw_url = "/index.html";

        auto resp = http::Client("localhost", BASE_PORT + 1, true, &socket_factory).make_request(req);

        BOOST_CHECK_EQUAL(200, resp.status.code);
        BOOST_CHECK_EQUAL("OK", resp.body);
    }

    server.exit();
    server_thread.join();
}

BOOST_AUTO_TEST_SUITE_END()
