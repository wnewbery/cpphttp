#pragma once
#include "net/Socket.hpp"
#include "TestSocketFactory.hpp"
#include <algorithm>
#include <string>
#include <cstring>

/**@brief A fake network socket using a string buffer.*/
class TestSocket : public http::Socket
{
public:
    TestSocket(TestSocketFactory *factory = nullptr)
        : factory(factory), tls(false), port(80)
        , recv_p(0), sent_last(false) {}
    ~TestSocket()
    {
        if (factory) factory->remove_socket(this);
    }

    virtual std::string address_str()const
    {
        return host;
    }

    virtual void disconnect() {}

    virtual size_t recv(void *buffer, size_t len)
    {
        if (sent_last)
        {
            recv_p = 0;
            sent_last = false;
        }
        len = std::min(len, recv_buffer.size() - recv_p);
        memcpy(buffer, recv_buffer.c_str() + recv_p, len);
        recv_p += len;
        return len;
    }

    virtual size_t send(const void *buffer, size_t len)
    {
        sent_last = true;
        send_buffer.append((const char*)buffer, len);
        return len;
    }

    void send_all(const void *buffer, size_t len)
    {
        send(buffer, len);
    }

    std::string recv_remaining()const
    {
        return { recv_buffer.begin() + recv_p, recv_buffer.end() };
    }

    TestSocketFactory *factory;
    bool tls;
    std::string host;
    uint16_t port;
    std::string recv_buffer;
    size_t recv_p;
    bool sent_last;
    std::string send_buffer;
};
