#include "net/OpenSslListenSocket.hpp"
#include "net/OpenSslSocket.hpp"
namespace http
{
    OpenSslListenSocket::OpenSslListenSocket(const std::string &bind, uint16_t port, const std::string &cert_hostname)
        : tcp(bind, port), cert_hostname(cert_hostname)
    {
    }
    OpenSslServerSocket OpenSslListenSocket::accept()
    {
        auto sock = tcp.accept();
        return OpenSslServerSocket(std::move(sock), cert_hostname);
    }
}
