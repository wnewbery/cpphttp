#include <boost/test/unit_test.hpp>
#include "net/TlsSocket.hpp"
#include "net/Net.hpp"

using namespace http;

BOOST_AUTO_TEST_SUITE(TestTlsSocket)

BOOST_AUTO_TEST_CASE(connect)
{
    const char *HTTP_REQ =
        "GET / HTTP/1.0\r\n"
        "Host: willnewbery.co.uk\r\n"
        "\r\n";
    TlsSocket sock;
    sock.connect("willnewbery.co.uk", 443);
    BOOST_CHECK_EQUAL("willnewbery.co.uk:443", sock.address_str());
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
