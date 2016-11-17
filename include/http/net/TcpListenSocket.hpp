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
        TcpListenSocket(TcpListenSocket &&mv) : socket(mv.socket)
        {
            mv.socket = INVALID_SOCKET;
        }
        TcpListenSocket& operator = (TcpListenSocket &&mv)
        {
            std::swap(socket, mv.socket);
            return *this;
        }

        SOCKET get() { return socket; }
        /**Sets this sockets non-blocking flag.*/
        void set_non_blocking(bool non_blocking=true);
        /**Accepts the next inbound connection, then returns it as a TcpSocket object.
         * If the socket is blocking, will wait for the next connection and always returns a
         * valid TcpSocket.
         * If the socket is non-blocking, an empty TcpSocket may be returned.
         */
        TcpSocket accept();
    private:
        SOCKET socket;
    };
}
