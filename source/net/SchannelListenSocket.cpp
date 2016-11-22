#include "net/SchannelListenSocket.hpp"
#include "net/SchannelSocket.hpp"
namespace http
{
    SchannelListenSocket::SchannelListenSocket(const std::string &bind, uint16_t port, const PrivateCert cert)
        : tcp(bind, port), cert(cert)
    {
    }
    SchannelServerSocket SchannelListenSocket::accept()
    {
        auto sock = tcp.accept();
        return SchannelServerSocket(std::move(sock), cert);
    }
}
