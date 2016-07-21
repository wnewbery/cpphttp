#pragma once
#include "Net.hpp"
#include "Socket.hpp"
#include "TcpSocket.hpp"

#include <openssl/opensslconf.h>
#include <openssl/ssl.h>
#ifndef OPENSSL_THREADS
#   error OPENSSL_THREADS required
#endif

namespace http
{
    std::string openssl_err_str(SSL *ssl, int error);
    
    class OpenSslSocketError : public SocketError
    {
    public:
        OpenSslSocketError(SSL *ssl, int error)
            : SocketError(openssl_err_str(ssl, error))
        {
        }
    };
    
    class OpenSslSocket : public Socket
    {
    public:
        OpenSslSocket();
        OpenSslSocket(const std::string &host, uint16_t port);
        virtual ~OpenSslSocket();

        void connect(const std::string &host, uint16_t port);

        virtual std::string address_str()const override;
        virtual void disconnect()override;
        virtual size_t recv(void *buffer, size_t len)override;
        virtual size_t send(const void *buffer, size_t len)override;
    private:
        TcpSocket tcp;
        SSL *ssl;
    };
}
