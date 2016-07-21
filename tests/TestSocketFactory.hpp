#pragma once
#include "client/SocketFactory.hpp"
#include "TestSocket.hpp"

class TestSocketFactory : public http::SocketFactory
{
public:
    std::string recv_buffer;
    TestSocket *last;

    virtual std::unique_ptr<http::Socket> connect(const std::string &host, uint16_t port, bool tls)override
    {
        auto sock = std::unique_ptr<TestSocket>(new TestSocket());
        sock->host = host;
        sock->port = port;
        sock->tls = tls;
        sock->recv_buffer = recv_buffer;
        last = sock.get();
        return std::move(sock);
    }
};


