#include <boost/test/unit_test.hpp>
#include "net/TlsListenSocket.hpp"
#include "net/TlsSocket.hpp"
#include "net/Net.hpp"
#include "../TestThread.hpp"
#include <iostream>
#include <thread>

using namespace http;

BOOST_AUTO_TEST_SUITE(TestTlsSocket)

static const uint16_t BASE_PORT = 5000;

void success_server_thread()
{
    SchannelListenSocket listen("127.0.0.1", BASE_PORT, "localhost");

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
        SchannelSocket sock("localhost", BASE_PORT);
        sock.send_all("ping", 4);

        char buffer[1024];
        auto len = sock.recv(buffer, sizeof(buffer));
        BOOST_CHECK_EQUAL(4, len);
        buffer[4] = 0;
        BOOST_CHECK_EQUAL("pong", buffer);
    }
    {
        SchannelSocket sock("localhost", BASE_PORT);
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

// Will fail because the test certificate is for "localhost" only, not "127.0.0.1"
void invalid_cert_hostname_server()
{
    SchannelListenSocket listen("127.0.0.1", BASE_PORT + 1, "localhost");
    BOOST_CHECK_THROW(listen.accept(), std::exception);
}
BOOST_AUTO_TEST_CASE(invalid_cert_hostname)
{
    TestThread server(&invalid_cert_hostname_server);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    BOOST_CHECK_THROW(SchannelSocket("127.0.0.1", BASE_PORT + 1), std::exception);
    server.join();
}

BOOST_AUTO_TEST_SUITE_END()
