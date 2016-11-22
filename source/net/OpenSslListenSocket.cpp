#include "net/OpenSslListenSocket.hpp"
#include "net/OpenSslSocket.hpp"
namespace http
{
    OpenSslListenSocket::OpenSslListenSocket(const std::string &bind, uint16_t port, const PrivateCert &cert)
        : tcp(bind, port), cert(cert)
    {
    }
    OpenSslServerSocket OpenSslListenSocket::accept()
    {
        auto sock = tcp.accept();
        return OpenSslServerSocket(std::move(sock), cert);
    }
}
