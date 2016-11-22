#pragma once
#include "TcpListenSocket.hpp"
#include "Cert.hpp"
#include <memory>
namespace http
{
    class SchannelServerSocket;
    /**TCP server side listen socket.*/
    class SchannelListenSocket
    {
    public:
        /**Listen on a port for a specific local interface address.
         * The host name is used to select a certificate from the users personal certificate store
         * by a matching common name.
         */
        SchannelListenSocket(const std::string &bind, uint16_t port, const PrivateCert cert);
        /**Listen on a port for all interfaces.*/
        explicit SchannelListenSocket(uint16_t port, const PrivateCert cert)
            : SchannelListenSocket("0.0.0.0", port, cert) {}
        SchannelListenSocket()=default;

        /**Blocks awaiting the next inbound connection, then returns it as a TcpSocket object.*/
        SchannelServerSocket accept();
    private:
        TcpListenSocket tcp;
        const PrivateCert cert;
    };
}
