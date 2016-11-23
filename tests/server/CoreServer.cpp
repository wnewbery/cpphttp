#include <boost/test/unit_test.hpp>
#include "client/Client.hpp"
#include "client/SocketFactory.hpp"
#include "server/CoreServer.hpp"
#include "net/Cert.hpp"
#include "net/Net.hpp"
#include "net/TcpSocket.hpp"
#include "Response.hpp"
#include "../TestThread.hpp"
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
    TestThread server_thread;
    Server server;
    server.add_tcp_listener("127.0.0.1", BASE_PORT + 0);
    server.add_tls_listener("127.0.0.1", BASE_PORT + 1, load_pfx_cert("localhost.pfx", "password"));
    server.add_tls_listener("127.0.0.1", BASE_PORT + 2, load_pfx_cert("wrong-host.pfx", "password"));

    server_thread = TestThread(std::bind(&Server::run, &server));

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
    {
        // Expected to fail since cant validate the certificate
        http::Request req;
        req.method = GET;
        req.headers.add("Host", "localhost");
        req.raw_url = "/index.html";
        BOOST_CHECK_THROW(
            http::Client("localhost", BASE_PORT + 2, true, &socket_factory).make_request(req),
            CertificateVerificationError);
    }
    server.exit();
    server_thread.join();
}

BOOST_AUTO_TEST_CASE(keep_alive)
{
    TestThread server_thread;
    Server server;
    server.add_tcp_listener("127.0.0.1", BASE_PORT + 3);

    server_thread = TestThread(std::bind(&Server::run, &server));

    {
        ClientConnection conn(std::make_unique<TcpSocket>("localhost", BASE_PORT + 3));
        Request req;
        req.method = GET;
        req.headers.add("Host", "localhost");
        req.headers.add("Connection", "close");
        req.raw_url = "/index.html";

        auto resp = conn.make_request(req);
        BOOST_CHECK_EQUAL("close", resp.headers.get("Connection"));

        BOOST_CHECK_THROW(conn.make_request(req), std::runtime_error);
    }

    {
        ClientConnection conn(std::make_unique<TcpSocket>("localhost", BASE_PORT + 3));
        Request req;
        req.method = GET;
        req.headers.add("Host", "localhost");
        req.headers.add("Connection", "keep-alive");
        req.raw_url = "/index.html";

        auto resp = conn.make_request(req);
        BOOST_CHECK_EQUAL("keep-alive", resp.headers.get("Connection"));

        BOOST_CHECK_NO_THROW(conn.make_request(req));
    }

    server.exit();
    server_thread.join();
}
BOOST_AUTO_TEST_SUITE_END()
