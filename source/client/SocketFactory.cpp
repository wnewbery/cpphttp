#include "client/SocketFactory.hpp"
#include "net/TcpSocket.hpp"
#include "net/TlsSocket.hpp"

namespace http
{
    std::unique_ptr<Socket> DefaultSocketFactory::connect(const std::string &host, uint16_t port, bool tls)
    {
        if (tls) return std::unique_ptr<Socket>(new TlsSocket(host, port));
        else return std::unique_ptr<Socket>(new TcpSocket(host, port));
    }
}
