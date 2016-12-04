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
#if _WIN32
    virtual SOCKET get()override { return 0; }
#else
    virtual http::SOCKET get()override { return 0; }
#endif
    virtual std::string address_str()const
    {
        return host;
    }

    virtual void close()override {}
    virtual void disconnect()override {}
    virtual bool check_recv_disconnect()override { return false; }

    virtual size_t recv(void *buffer, size_t len)override
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

    virtual size_t send(const void *buffer, size_t len)override
    {
        sent_last = true;
        send_buffer.append((const char*)buffer, len);
        return len;
    }

    std::string recv_remaining()const
    {
        return { recv_buffer.begin() + recv_p, recv_buffer.end() };
    }

    virtual void async_disconnect(http::AsyncIo &,
        std::function<void()>, http::AsyncIo::ErrorHandler)override {}
    virtual void async_recv(http::AsyncIo &, void *, size_t,
        http::AsyncIo::RecvHandler, http::AsyncIo::ErrorHandler)override {}
    virtual void async_send(http::AsyncIo &, const void *, size_t,
        http::AsyncIo::SendHandler, http::AsyncIo::ErrorHandler)override {}
    virtual void async_send_all(http::AsyncIo &, const void *, size_t,
        http::AsyncIo::SendHandler, http::AsyncIo::ErrorHandler)override {}

    TestSocketFactory *factory;
    bool tls;
    std::string host;
    uint16_t port;
    std::string recv_buffer;
    size_t recv_p;
    bool sent_last;
    std::string send_buffer;
};
