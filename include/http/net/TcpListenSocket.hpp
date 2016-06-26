#pragma once
#include "Os.hpp"
#include <cstdint>
#include <string>
namespace http
{
    class TcpSocket;
    class TcpListenSocket
    {
    public:
        TcpListenSocket(const std::string &bind, uint16_t port);
        TcpListenSocket(uint16_t port) : TcpListenSocket("0.0.0.0", port) {}
        TcpListenSocket();

        TcpSocket accept();
    private:
        SOCKET socket;
    };
}
