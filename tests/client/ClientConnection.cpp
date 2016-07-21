#include <boost/test/unit_test.hpp>
#include "client/ClientConnection.hpp"
#include "../TestSocket.hpp"
using namespace http;

BOOST_AUTO_TEST_SUITE(TestClientConnection)


BOOST_AUTO_TEST_CASE(test)
{
    std::unique_ptr<TestSocket> tmp(new TestSocket());
    auto socket = tmp.get();
    ClientConnection conn(std::move(tmp));

    socket->recv_buffer = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "0123456789";

    Request req;
    req.method = GET;
    req.headers.add("Host", "localhost");
    req.raw_url = "/index.html";

    auto resp = conn.make_request(req);
    //TODO: Should check socket->send_buffer, but Date header and undefined order makes this tricky
    BOOST_CHECK_EQUAL("", socket->recv_remaining());
    BOOST_CHECK_EQUAL(SC_OK, resp.status.code);
    BOOST_CHECK_EQUAL("OK", resp.status.msg);
    BOOST_CHECK_EQUAL("text/plain", resp.headers.get("Content-Type"));
    BOOST_CHECK_EQUAL("0123456789", resp.body);
}


BOOST_AUTO_TEST_SUITE_END()
