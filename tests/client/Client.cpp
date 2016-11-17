#include <boost/test/unit_test.hpp>
#include "client/Client.hpp"
#include "../TestSocket.hpp"
#include "../TestSocketFactory.hpp"
using namespace http;

BOOST_AUTO_TEST_SUITE(TestClient)


BOOST_AUTO_TEST_CASE(test)
{
    TestSocketFactory socket_factory;
    socket_factory.recv_buffer =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "0123456789";

    Client client("localhost", 80, false, &socket_factory);
    client.def_headers().set("User-Agent", "test");

    {
        Request req;
        req.method = GET;
        req.raw_url = "/index.html";

        auto resp = client.make_request(req);

        BOOST_CHECK_EQUAL("", socket_factory.last->recv_remaining());
        BOOST_CHECK_EQUAL("localhost:80", req.headers.get("Host"));
        BOOST_CHECK_EQUAL("test", req.headers.get("User-Agent"));

        BOOST_CHECK_EQUAL(SC_OK, resp.status.code);
        BOOST_CHECK_EQUAL("OK", resp.status.msg);
        BOOST_CHECK_EQUAL("text/plain", resp.headers.get("Content-Type"));
        BOOST_CHECK_EQUAL("0123456789", resp.body);
    }
    //connection reuse
    socket_factory.last->recv_buffer =
        "HTTP/1.1 204 No Content\r\n"
        "Server: example\r\n"
        "\r\n";
    {
        Request req;
        req.method = POST;
        req.raw_url = "/example";
        req.body = {'X'};

        auto resp = client.make_request(req);

        BOOST_CHECK_EQUAL("", socket_factory.last->recv_remaining());
        BOOST_CHECK_EQUAL("localhost:80", req.headers.get("Host"));
        BOOST_CHECK_EQUAL("test", req.headers.get("User-Agent"));
        BOOST_CHECK_EQUAL("1", req.headers.get("Content-Length"));

        BOOST_CHECK_EQUAL(SC_NO_CONTENT, resp.status.code);
        BOOST_CHECK_EQUAL("No Content", resp.status.msg);
        BOOST_CHECK_EQUAL("", resp.body);
    }
}


BOOST_AUTO_TEST_SUITE_END()
