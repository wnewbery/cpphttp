#pragma once
#include "net/Socket.hpp"
#include <algorithm>
#include <string>
#include <cstring>

/**@brief A fake network socket using a string buffer.*/
class TestSocket : public http::Socket
{
public:
    TestSocket() : tls(false), port(80) {}

    virtual std::string address_str()const
    {
        return host;
    }

    virtual void disconnect() {}

    virtual size_t recv(void *buffer, size_t len)
    {
        len = std::min(len, recv_buffer.size());
        memcpy(buffer, recv_buffer.c_str(), len);
        recv_buffer.erase(0, len);
        return len;
    }

    virtual size_t send(const void *buffer, size_t len)
    {
        send_buffer.append((const char*)buffer, len);
        return len;
    }

    void send_all(const void *buffer, size_t len)
    {
        send(buffer, len);
    }

    bool tls;
    std::string host;
    uint16_t port;
    std::string recv_buffer;
    std::string send_buffer;
};
