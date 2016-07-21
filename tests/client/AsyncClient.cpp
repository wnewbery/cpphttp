#include <boost/test/unit_test.hpp>
#include "client/AsyncClient.hpp"
#include "../TestSocket.hpp"
#include "../TestSocketFactory.hpp"
#include "net/Net.hpp"
#include <chrono>
using namespace http;

BOOST_AUTO_TEST_SUITE(TestClient)

BOOST_AUTO_TEST_CASE(construct)
{
    TestSocketFactory socket_factory;
    socket_factory.recv_buffer =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "0123456789";

    AsyncClientParams params;
    params.host = "localhost";
    params.port = 80;
    params.tls = false;
    params.max_connections = 5;
    params.socket_factory = &socket_factory;

    AsyncClient client(params);

    client.exit();
    client.start();
    client.exit();
}

BOOST_AUTO_TEST_CASE(single)
{
    TestSocketFactory socket_factory;
    socket_factory.recv_buffer =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "0123456789";

    AsyncClientParams params;
    params.host = "localhost";
    params.port = 80;
    params.tls = false;
    params.max_connections = 5;
    params.socket_factory = &socket_factory;

    AsyncClient client(params);

    AsyncRequest req;
    req.method = GET;
    req.raw_url = "/index.html";

    auto response = client.make_request(&req).get();
    BOOST_CHECK_EQUAL(200, response->status.code);
}

BOOST_AUTO_TEST_CASE(sequential)
{
    TestSocketFactory socket_factory;
    socket_factory.recv_buffer =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "0123456789";

    AsyncClientParams params;
    params.host = "localhost";
    params.port = 80;
    params.tls = false;
    params.max_connections = 5;
    params.socket_factory = &socket_factory;

    AsyncClient client(params);

    AsyncRequest req;
    req.method = GET;
    req.raw_url = "/index.html";

    auto response = client.make_request(&req).get();
    BOOST_CHECK_EQUAL(200, response->status.code);

    response = client.make_request(&req).get();
    BOOST_CHECK_EQUAL(200, response->status.code);

    response = client.make_request(&req).get();
    BOOST_CHECK_EQUAL(200, response->status.code);
}


BOOST_AUTO_TEST_CASE(callback)
{
    std::atomic<bool> done(false);

    TestSocketFactory socket_factory;
    socket_factory.recv_buffer =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "0123456789";

    AsyncClientParams params;
    params.host = "localhost";
    params.port = 80;
    params.tls = false;
    params.max_connections = 5;
    params.socket_factory = &socket_factory;

    AsyncClient client(params);

    AsyncRequest req;
    req.method = GET;
    req.raw_url = "/index.html";
    req.on_completion = [&done](AsyncRequest *, Response &) -> void
    {
        done = true;
    };

    client.make_request(&req);
    auto response = req.wait();
    BOOST_CHECK_EQUAL(200, response->status.code);
    BOOST_CHECK(done);
}

BOOST_AUTO_TEST_CASE(async_error)
{
    std::atomic<bool> done(false);

    class Factory : public SocketFactory
    {
    public:
        virtual std::unique_ptr<http::Socket> connect(const std::string &host, uint16_t port, bool)override
        {
            throw ConnectionError(host, port);
        }
    };
    Factory socket_factory;

    AsyncClientParams params;
    params.host = "localhost";
    params.port = 80;
    params.tls = false;
    params.max_connections = 5;
    params.socket_factory = &socket_factory;

    AsyncClient client(params);

    AsyncRequest req;
    req.method = GET;
    req.raw_url = "/index.html";
    req.on_exception = [&done](AsyncRequest*) -> void
    {
        try
        {
            throw;
        }
        catch (const ConnectionError &)
        {
            done = true;
        }
    };

    client.make_request(&req);
    BOOST_CHECK_THROW(req.wait(), ConnectionError);
    BOOST_CHECK(done);
}

BOOST_AUTO_TEST_CASE(parallel)
{
    class Factory : public TestSocketFactory
    {
    public:
        using TestSocketFactory::TestSocketFactory;
        std::mutex go;
        virtual std::unique_ptr<http::Socket> connect(const std::string &host, uint16_t port, bool tls)override
        {
            ++connect_count;
            std::unique_lock<std::mutex> lock(go);
            auto sock = std::unique_ptr<TestSocket>(new TestSocket());
            sock->host = host;
            sock->port = port;
            sock->tls = tls;
            sock->recv_buffer = recv_buffer;
            last = sock.get();
            return std::move(sock);
        }
    };
    Factory socket_factory;
    socket_factory.recv_buffer =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "0123456789";

    std::unique_lock<std::mutex> lock(socket_factory.go);

    AsyncClientParams params;
    params.host = "localhost";
    params.port = 80;
    params.tls = false;
    params.max_connections = 4;
    params.socket_factory = &socket_factory;

    AsyncClient client(params);

    auto req = []() -> AsyncRequest
    {
        AsyncRequest req;
        req.method = GET;
        req.raw_url = "/index.html";
        return req;
    };

    auto a = req();
    auto b = req();
    auto c = req();
    auto d = req();
    auto e = req();
    auto f = req();

    client.make_request(&a);
    client.make_request(&b);
    client.make_request(&c);
    client.make_request(&d);
    client.make_request(&e);
    client.make_request(&f);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    BOOST_CHECK_EQUAL(4U, socket_factory.connect_count.load());
    lock.unlock();

    BOOST_CHECK_NO_THROW(a.wait());
    BOOST_CHECK_NO_THROW(b.wait());
    BOOST_CHECK_NO_THROW(c.wait());
    BOOST_CHECK_NO_THROW(d.wait());
    BOOST_CHECK_NO_THROW(e.wait());
    BOOST_CHECK_NO_THROW(f.wait());

    BOOST_CHECK_EQUAL(4U, socket_factory.connect_count.load());
}


BOOST_AUTO_TEST_SUITE_END()
