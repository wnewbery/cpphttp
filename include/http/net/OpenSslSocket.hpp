#pragma once
#include "Net.hpp"
#include "Socket.hpp"
#include "TcpSocket.hpp"
#include "OpenSsl.hpp"
#include <memory>

namespace http
{
    class PrivateCert;

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

        virtual void async_disconnect(http::AsyncIo &,
            std::function<void()>, http::AsyncIo::ErrorHandler)override {}
        virtual void async_recv(http::AsyncIo &, void *, size_t,
            http::AsyncIo::RecvHandler, http::AsyncIo::ErrorHandler)override {}
        virtual void async_send(http::AsyncIo &, const void *, size_t,
            http::AsyncIo::SendHandler, http::AsyncIo::ErrorHandler)override {}
        virtual void async_send_all(http::AsyncIo &, const void *, size_t,
            http::AsyncIo::SendHandler, http::AsyncIo::ErrorHandler)override {}

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
        OpenSslServerSocket() : OpenSslSocket() {}
        OpenSslServerSocket(TcpSocket &&socket, const PrivateCert &cert);
        OpenSslServerSocket(const OpenSslServerSocket&)=delete;
        OpenSslServerSocket(OpenSslServerSocket&&)=default;
        OpenSslServerSocket& operator =(const OpenSslServerSocket&)=delete;
        OpenSslServerSocket& operator =(OpenSslServerSocket&&)=default;

        void async_create(AsyncIo &, TcpSocket &&, const PrivateCert &,
            std::function<void()>, AsyncIo::ErrorHandler) {}
    protected:
        std::unique_ptr<SSL_CTX, SslDeleter> openssl_ctx;
    };
}
