#include "net/SchannelListenSocket.hpp"
#include "net/SchannelSocket.hpp"
namespace http
{
    SchannelListenSocket::SchannelListenSocket(const std::string &bind, uint16_t port, const std::string &cert_hostname)
        : tcp(bind, port), cert_hostname(cert_hostname)
    {
    }
    SchannelServerSocket SchannelListenSocket::accept()
    {
        auto sock = tcp.accept();
        return SchannelServerSocket(std::move(sock), cert_hostname);
    }
}
