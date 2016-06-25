#pragma once
#include "Socket.hpp"
#include "Os.hpp"
#include <cstdint>
namespace http
{
    class OpenSslSocket : public Socket
    {
    public:
        OpenSslSocket();
        OpenSslSocket(const std::string &host, uint16_t port);
        virtual ~OpenSslSocket() {}

        void connect(const std::string &host, uint16_t port);

        virtual std::string address_str()const override;
        virtual void disconnect()override;
        virtual size_t recv(void *buffer, size_t len)override;
        virtual size_t send(const void *buffer, size_t len)override;
    private:
        SOCKET socket;
    };
}
