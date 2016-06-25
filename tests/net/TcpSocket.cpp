#include <boost/test/unit_test.hpp>
#include "net/TcpSocket.hpp"
#include "net/Net.hpp"

using namespace http;

BOOST_AUTO_TEST_SUITE(TestTcpSocket)
BOOST_AUTO_TEST_CASE(invalid_host)
{
    TcpSocket sock;
    BOOST_CHECK_THROW(sock.connect("not#a@valid@domain", 80), NetworkError);
}

//TODO: this is more difficult to test. Some annoying ISP's like BT like to return a "fake" DNS record to insert there own web pages with the default DHCP DNS servers...
// The result of this is that the address lookup, the thing intending to test, does succeed
// picking a port such ISP's dont open would fail, but is still not testing the intended thing
/*BOOST_AUTO_TEST_CASE(unknown_host)
{
    TcpSocket sock;
    BOOST_CHECK_THROW(sock.connect("does-not-exist.willnewbery.co.uk", 80), NetworkError);
}*/
BOOST_AUTO_TEST_CASE(no_connect)
{
    //Assumes 3432 is an unused port
    TcpSocket sock;
    BOOST_CHECK_THROW(sock.connect("localhost", 3432), NetworkError);
}

BOOST_AUTO_TEST_CASE(connect)
{
    //TODO: Come up with something better to connect to
    const char *HTTP_REQ =
        "GET / HTTP/1.0\r\n"
        "Host: willnewbery.co.uk\r\n"
        "\r\n";
    TcpSocket sock;
    sock.connect("willnewbery.co.uk", 80);
    BOOST_CHECK_EQUAL("willnewbery.co.uk", sock.host());
    BOOST_CHECK_EQUAL(80, sock.port());
    BOOST_CHECK_EQUAL("willnewbery.co.uk:80", sock.address_str());
    sock.send_all((const uint8_t*)HTTP_REQ, strlen(HTTP_REQ));

    size_t total = 0;
    size_t max = 1024 * 1024;
    char buffer[4096];
    while (auto len = sock.recv((uint8_t*)buffer, sizeof(buffer)))
    {
        total += len;
        if (total > max) BOOST_FAIL("Received more data than expected");
    }
    BOOST_CHECK(total > 0);

    sock.disconnect();
}

BOOST_AUTO_TEST_SUITE_END()
