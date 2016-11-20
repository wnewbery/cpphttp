#include <boost/test/unit_test.hpp>
#include "net/TlsListenSocket.hpp"
#include "net/TlsSocket.hpp"
#include "net/Net.hpp"
#include "../TestThread.hpp"
#include <thread>

using namespace http;

BOOST_AUTO_TEST_SUITE(TestTlsServer)

static const uint16_t BASE_PORT = 5000;

void success_server_thread()
{
    TlsListenSocket listen("127.0.0.1", BASE_PORT, "localhost");

    {
        auto sock = listen.accept();

        char buffer[1024];
        auto len = sock.recv(buffer, sizeof(buffer));
        BOOST_CHECK_EQUAL(4, len);
        buffer[4] = 0;
        BOOST_CHECK_EQUAL("ping", buffer);

        sock.send_all("pong", 4);

        sock.disconnect();
    }
    {
        auto sock = listen.accept();
        sock.send_all("pong", 4);

        char buffer[1024];
        auto len = sock.recv(buffer, sizeof(buffer));
        BOOST_CHECK_EQUAL(4, len);
        buffer[4] = 0;
        BOOST_CHECK_EQUAL("ping", buffer);

        sock.disconnect();
    }
}
BOOST_AUTO_TEST_CASE(success)
{
    TestThread server(&success_server_thread);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    {
        TlsSocket sock("localhost", BASE_PORT);
        sock.send_all("ping", 4);

        char buffer[1024];
        auto len = sock.recv(buffer, sizeof(buffer));
        BOOST_CHECK_EQUAL(4, len);
        buffer[4] = 0;
        BOOST_CHECK_EQUAL("pong", buffer);
    }
    {
        TlsSocket sock("localhost", BASE_PORT);
        char buffer[1024];
        auto len = sock.recv(buffer, sizeof(buffer));
        BOOST_CHECK_EQUAL(4, len);
        buffer[4] = 0;
        BOOST_CHECK_EQUAL("pong", buffer);

        sock.send_all("ping", 4);
        sock.disconnect();
    }
    server.join();
}

void invalid_cert_hostname_server()
{
    TlsListenSocket listen("127.0.0.1", BASE_PORT + 1, "wrong-hostname");
    BOOST_CHECK_THROW(listen.accept(), std::exception);
}
BOOST_AUTO_TEST_CASE(invalid_cert_hostname)
{
    TestThread server(&invalid_cert_hostname_server);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    BOOST_CHECK_THROW(TlsSocket("localhost", BASE_PORT + 1), std::exception);
    server.join();
}

BOOST_AUTO_TEST_SUITE_END()
