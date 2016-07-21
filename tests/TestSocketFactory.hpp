#pragma once
#include "client/SocketFactory.hpp"
#include <atomic>
#include <mutex>
#include <set>

class TestSocket;
class TestSocketFactory : public http::SocketFactory
{
public:
    TestSocketFactory()
        : recv_buffer(), last(nullptr), connect_count(0)
    {}
    ~TestSocketFactory();

    std::string recv_buffer;
    TestSocket *last;
    std::atomic<unsigned> connect_count;
    std::mutex mutex;
    std::set<TestSocket*> alive_sockets;

    void remove_socket(TestSocket *sock);
    virtual std::unique_ptr<http::Socket> connect(const std::string &host, uint16_t port, bool tls)override;
};


