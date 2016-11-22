#pragma once
#include "TcpListenSocket.hpp"
#include "net/Cert.hpp"
namespace http
{
    class OpenSslServerSocket;
    /**TCP server side listen socket.*/
    class OpenSslListenSocket
    {
    public:
        /**Listen on a port for a specific local interface address.
         * The host name is used to select a certificate from the users personal certificate store
         * by a matching common name.
         */
        OpenSslListenSocket(const std::string &bind, uint16_t port, const PrivateCert &cert);
        /**Listen on a port for all interfaces.*/
        explicit OpenSslListenSocket(uint16_t port, const PrivateCert &cert)
            : OpenSslListenSocket("0.0.0.0", port, cert) {}
        OpenSslListenSocket()=default;

        /**Blocks awaiting the next inbound connection, then returns it as a TcpSocket object.*/
        OpenSslServerSocket accept();
    private:
        TcpListenSocket tcp;
        PrivateCert cert;
    };
}
