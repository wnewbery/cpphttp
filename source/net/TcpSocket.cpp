#include "net/TcpSocket.hpp"
#include "net/Os.hpp"
#include "net/Net.hpp"
#include "net/SocketUtils.hpp"
#include <limits>
#include <cassert>
namespace http
{
    TcpSocket::TcpSocket()
        : socket(INVALID_SOCKET), _host(), _port(0)
    {
    }
    TcpSocket::TcpSocket(const std::string & host, uint16_t port)
        : socket(INVALID_SOCKET), _host(), _port(0)
    {
        connect(host, port);
    }
    TcpSocket::TcpSocket(SOCKET socket, const sockaddr *address)
        : socket(INVALID_SOCKET), _host(), _port(0)
    {
        set_socket(socket, address);
    }
    TcpSocket::~TcpSocket()
    {
        close();
    }

    TcpSocket::TcpSocket(TcpSocket &&mv)
        : socket(), _host(std::move(mv._host)), _port(mv._port)
    {
        socket = mv.socket;
        mv.socket = INVALID_SOCKET;
    }
    TcpSocket& TcpSocket::operator = (TcpSocket &&mv)
    {
        socket = mv.socket;
        mv.socket = INVALID_SOCKET;
        _host = std::move(mv._host);
        _port = mv._port;
        return *this;
    }

    void TcpSocket::set_socket(SOCKET _socket, const sockaddr *address)
    {
        if (socket != INVALID_SOCKET) throw std::runtime_error("Already connected");
        if (address->sa_family == AF_INET)
        {
            auto addr4 = (sockaddr_in*)address;
            char ipstr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr4->sin_addr, ipstr, sizeof ipstr);
            _port = ntohs(addr4->sin_port);
            _host = ipstr;
        }
        else if (address->sa_family == AF_INET6)
        {
            auto addr6 = (sockaddr_in6*)address;
            char ipstr[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &addr6->sin6_addr, ipstr, sizeof ipstr);
            _port = ntohs(addr6->sin6_port);
            _host = ipstr;
        }
        else throw std::runtime_error("Unknown socket family");
        socket = _socket;
    }
    void TcpSocket::connect(const std::string & host, uint16_t port)
    {
        struct AddrInfoPtr
        {
            addrinfo *p;
            AddrInfoPtr() : p(nullptr) {}
            ~AddrInfoPtr() { freeaddrinfo (p); }
        };

        if (socket != INVALID_SOCKET) throw std::runtime_error("Already connected");

        auto port_str = std::to_string(port);
        AddrInfoPtr result;

        addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        auto ret = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result.p);
        if (ret) throw ConnectionError(host, port);
        assert(result.p);

        int last_error = 0;
        for (auto p = result.p; p; p = p->ai_next)
        {
            socket = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (socket == INVALID_SOCKET) continue;

            if (::connect(socket, p->ai_addr, (int)p->ai_addrlen) != SOCKET_ERROR)
            {
                this->_host = host;
                this->_port = port;
                return;
            }
            else
            {
                last_error = last_net_error();
                closesocket(socket);
                continue;
            }

        }
        //TODO: Better error report if there were multiple possible address
        throw ConnectionError(last_error, host, port);
    }
    void TcpSocket::set_non_blocking(bool non_blocking)
    {
        http::set_non_blocking(socket, non_blocking);
    }
    std::string TcpSocket::address_str() const
    {
        if (socket != INVALID_SOCKET) return _host + ":" + std::to_string(_port);
        else return "Not connected";
    }
    void TcpSocket::close()
    {
        if (socket != INVALID_SOCKET)
        {
            closesocket(socket);
            socket = INVALID_SOCKET;
        }
    }
    void TcpSocket::disconnect()
    {
        if (socket != INVALID_SOCKET)
        {
            shutdown(socket, SD_SEND);
            fd_set set;
            FD_ZERO(&set);
            FD_SET(socket, &set);
            timeval timeout = {1, 0};
            // Give the remote a chance to send its FIN response
            select((int)socket + 1, &set, nullptr, nullptr, &timeout);
            //char buffer[1];
            //FD_ISSET(socket, &set) ::recv(socket, buffer, 1, 0) == 0)

            closesocket(socket);
            socket = INVALID_SOCKET;
        }
    }
    size_t TcpSocket::recv(void * buffer, size_t len)
    {
        if (len > std::numeric_limits<int>::max()) len = std::numeric_limits<int>::max();
        auto ret = ::recv(socket, (char*)buffer, (int)len, 0);
        if (ret < 0)
        {
            auto err = last_net_error();
            throw SocketError(err);
        }
        return (size_t)ret;
    }
    size_t TcpSocket::send(const void * buffer, size_t len)
    {
        if (len > std::numeric_limits<int>::max()) len = std::numeric_limits<int>::max();
        auto ret = ::send(socket, (const char*)buffer, (int)len, 0);
        if (ret < 0)
        {
            auto err = last_net_error();
            throw SocketError(err);
        }
        return (size_t)ret;
    }

    bool TcpSocket::check_recv_disconnect()
    {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(socket, &set);
        timeval timeout = {0, 0};
        auto ret = ::select((int)socket + 1, &set, nullptr, nullptr, &timeout);
        if (ret == 0) return false;
        else if (ret < 0)
        {
            auto err = last_net_error();
            throw SocketError(err);
        }
        assert(ret == 1);

        char buffer[1];
        if (recv(buffer, sizeof(buffer)) == 0) return true;
        else throw std::runtime_error("Received unexpected data.");
    }
    void TcpSocket::async_disconnect(AsyncIo &aio,
        std::function<void()> handler, AsyncIo::ErrorHandler error)
    {
        assert(socket != INVALID_SOCKET);
        shutdown(socket, SD_SEND);

        static char buffer[1];
        aio.recv(socket, buffer, 1,
            [this, handler](size_t)
            {
                closesocket(socket);
                socket = INVALID_SOCKET;
                handler();
            },
            [this, error]()
            {
                closesocket(socket);
                socket = INVALID_SOCKET;
                error();
            });
    }
    void TcpSocket::async_recv(AsyncIo &aio, void *buffer, size_t len,
        AsyncIo::RecvHandler handler, AsyncIo::ErrorHandler error)
    {
        aio.recv(socket, buffer, len, handler, error);
    }
    void TcpSocket::async_send(AsyncIo &aio, const void *buffer, size_t len,
        AsyncIo::SendHandler handler, AsyncIo::ErrorHandler error)
    {
        aio.send(socket, buffer, len, handler, error);
    }
    void TcpSocket::async_send_all(AsyncIo &aio, const void *buffer, size_t len,
        AsyncIo::SendHandler handler, AsyncIo::ErrorHandler error)
    {
        aio.send_all(socket, buffer, len, handler, error);
    }
}
