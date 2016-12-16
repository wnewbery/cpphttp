#include "net/TcpListenSocket.hpp"
#include "net/Net.hpp"
#include "net/TcpSocket.hpp"
#include "net/SocketUtils.hpp"
namespace http
{
    TcpListenSocket::TcpListenSocket(const std::string &bind, uint16_t port)
        : socket(INVALID_SOCKET)
    {
        socket = create_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket == INVALID_SOCKET) throw std::runtime_error("Failed to create listen socket");
        //allow fast restart
#ifndef _WIN32
        int yes = 1;
        setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
#endif
        //bind socket
        //TODO: Support IPv6
        sockaddr_in bind_addr = { 0 };
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, bind.c_str(), &bind_addr.sin_addr) != 1)
            throw std::runtime_error("Invalid bind address: " + bind);

        if (::bind(socket, (const sockaddr*)&bind_addr, sizeof(bind_addr)))
            throw std::runtime_error("Failed to bind listen socket to " + bind + ":" + std::to_string(port));
        //listen
        if (::listen(socket, 10))
            throw std::runtime_error("Socket listen failed for " + bind + ":" + std::to_string(port));
    }
    TcpListenSocket::TcpListenSocket()
    {
        closesocket(socket);
    }
    void TcpListenSocket::set_non_blocking(bool non_blocking)
    {
        http::set_non_blocking(socket, non_blocking);
    }
    TcpSocket TcpListenSocket::accept()
    {
        sockaddr_storage client_addr = { 0 };
        socklen_t client_addr_len = (socklen_t)sizeof(client_addr);
        auto client_socket = ::accept(socket, (sockaddr*)&client_addr, &client_addr_len);
        if (client_socket == INVALID_SOCKET)
            throw SocketError("socket accept failed", last_net_error());
        return { client_socket, (sockaddr*)&client_addr };
    }
}
