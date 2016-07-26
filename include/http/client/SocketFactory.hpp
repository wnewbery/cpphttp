#pragma once
#include <memory>
#include <string>
#include <cstdint>
namespace http
{
    class Socket;
    /**Factory interface for creating client socket connections.*/
    class SocketFactory
    {
    public:
        virtual ~SocketFactory() {}
        /**Connect to a remote client.
         * @param host DNS name or IP of host to connect to.
         * @param port Port number to connect to.
         * @param tls Socket is required to use TLS.
         * @return Connected socket ready to send and receieve.
         * @throws NetworkError if connection fails
         */
        virtual std::unique_ptr<Socket> connect(const std::string &host, uint16_t port, bool tls)=0;
    };

    /**Factory using TcpSocket and TlsSocket.*/
    class DefaultSocketFactory : public SocketFactory
    {
    public:
        virtual std::unique_ptr<Socket> connect(const std::string &host, uint16_t port, bool tls)override;
    };
}
