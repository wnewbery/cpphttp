#include "TestSocketFactory.hpp"
#include "TestSocket.hpp"

TestSocketFactory::~TestSocketFactory()
{
}

std::unique_ptr<http::Socket> TestSocketFactory::connect(const std::string &host, uint16_t port, bool tls)
{
    ++connect_count;

    std::unique_lock<std::mutex> lock(mutex);
    auto sock = std::unique_ptr<TestSocket>(new TestSocket(this));
    alive_sockets.insert(sock.get());
    sock->host = host;
    sock->port = port;
    sock->tls = tls;
    sock->recv_buffer = recv_buffer;
    last = sock.get();
    return std::move(sock);
}

void TestSocketFactory::remove_socket(TestSocket *sock)
{
    std::unique_lock<std::mutex> lock(mutex);
    alive_sockets.erase(sock);
}
