#pragma once
#include "Net.hpp"
#include "Socket.hpp"
#include "TcpSocket.hpp"
#include <memory>
#include <openssl/opensslconf.h>
#include <openssl/ssl.h>
#ifndef OPENSSL_THREADS
#   error OPENSSL_THREADS required
#endif

namespace http
{
    /**Get a descriptive error string for an OpenSSL error code.*/
    std::string openssl_err_str(SSL *ssl, int error);

    /**Errors from OpenSSL.*/
    class OpenSslSocketError : public SocketError
    {
    public:
        /**Create an error message using openssl_err_str.*/
        OpenSslSocketError(SSL *ssl, int error)
            : SocketError(openssl_err_str(ssl, error))
        {
        }
    };

    /**TLS secure socket using OpenSSL.*/
    class OpenSslSocket : public Socket
    {
    public:
        OpenSslSocket();
        /**Establish a client connection to a specific host and port.*/
        OpenSslSocket(const std::string &host, uint16_t port);
        virtual ~OpenSslSocket();

        virtual SOCKET get()override { return tcp.get(); }
        /**Establish a client connection to a specific host and port.*/
        void connect(const std::string &host, uint16_t port);

        virtual std::string address_str()const override;
        virtual void disconnect()override;
        virtual size_t recv(void *buffer, size_t len)override;
        virtual size_t send(const void *buffer, size_t len)override;
        virtual bool check_recv_disconnect()override;
    private:
        struct SslDeleter
        {
            void operator()(SSL *ssl)const;
        };
        TcpSocket tcp;
        std::unique_ptr<SSL, SslDeleter> ssl;
    };
}
