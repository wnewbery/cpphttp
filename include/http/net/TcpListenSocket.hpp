#pragma once
#include "Os.hpp"
#include <cstdint>
#include <string>
namespace http
{
    class TcpSocket;
    /**TCP server side listen socket.*/
    class TcpListenSocket
    {
    public:
        /**Listen on a port for a specific local interface address.*/
        TcpListenSocket(const std::string &bind, uint16_t port);
        /**Listen on a port for all interfaces.*/
        explicit TcpListenSocket(uint16_t port) : TcpListenSocket("0.0.0.0", port) {}
        TcpListenSocket();

        /**Blocks awaiting the next inbound connection, then returns it as a TcpSocket object.*/
        TcpSocket accept();
    private:
        SOCKET socket;
    };
}
