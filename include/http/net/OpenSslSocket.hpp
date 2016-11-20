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
        OpenSslSocket(const OpenSslSocket&)=delete;
        OpenSslSocket(OpenSslSocket &&mv)=default;
        virtual ~OpenSslSocket();
        OpenSslSocket& operator = (const OpenSslSocket&)=delete;
        OpenSslSocket& operator = (OpenSslSocket &&mv)=default;

        virtual SOCKET get()override { return tcp.get(); }
        /**Establish a client connection to a specific host and port.*/
        void connect(const std::string &host, uint16_t port);

        virtual std::string address_str()const override;
        virtual void close()override;
        virtual void disconnect()override;
        virtual bool recv_pending()const override;
        virtual size_t recv(void *buffer, size_t len)override;
        virtual size_t send(const void *buffer, size_t len)override;
        virtual bool check_recv_disconnect()override;
    protected:
        struct SslDeleter
        {
            void operator()(SSL *ssl)const;
            void operator()(SSL_CTX *ctx)const
            {
                if (ctx) SSL_CTX_free(ctx);
            }
        };
        TcpSocket tcp;
        std::unique_ptr<SSL, SslDeleter> ssl;
    };
    /**Server side OpenSSL socket. Presents a certificate on connection.*/
    class OpenSslServerSocket : public OpenSslSocket
    {
    public:
        OpenSslServerSocket(TcpSocket &&socket, const std::string &cert_hostname);
        OpenSslServerSocket(const OpenSslServerSocket&)=delete;
        OpenSslServerSocket(OpenSslServerSocket&&)=default;
        OpenSslServerSocket& operator =(const OpenSslServerSocket&)=delete;
        OpenSslServerSocket& operator =(OpenSslServerSocket&&)=default;
    protected:
        std::unique_ptr<SSL_CTX, SslDeleter> openssl_ctx;
    };
}
