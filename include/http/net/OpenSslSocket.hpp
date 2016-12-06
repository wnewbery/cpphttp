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

        virtual void async_disconnect(AsyncIo &aio,
            std::function<void()> handler, AsyncIo::ErrorHandler error)override;
        virtual void async_recv(AsyncIo &aio, void *buffer, size_t len,
            AsyncIo::RecvHandler handler, AsyncIo::ErrorHandler error)override;
        virtual void async_send(AsyncIo &aio, const void *buffer, size_t len,
            AsyncIo::SendHandler handler, AsyncIo::ErrorHandler error)override;
        virtual void async_send_all(AsyncIo &aio, const void *buffer, size_t len,
            AsyncIo::SendHandler handler, AsyncIo::ErrorHandler error)override;
    protected:
        TcpSocket tcp;
        std::unique_ptr<SSL, detail::OpenSslDeleter> ssl;
        /**Received encrpyted data ready for OpenSSL. Owned by ssl.*/
        BIO *in_bio;
        /**Outgoing encrypted data ready to send to the remote. Owned by ssl.*/
        BIO *out_bio;
        char bio_buffer[4096];

        void async_send_next(AsyncIo &aio, const void *buffer, size_t len, size_t sent,
            AsyncIo::SendHandler handler, AsyncIo::ErrorHandler error);
        void async_send_bio(AsyncIo &aio, std::function<void()> handler, AsyncIo::ErrorHandler error);
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

        void async_create(AsyncIo &aio, TcpSocket &&socket, const PrivateCert &cert,
            std::function<void()> handler, AsyncIo::ErrorHandler error);
    private:
        std::unique_ptr<SSL_CTX, detail::OpenSslDeleter> openssl_ctx;

        void setup(TcpSocket &&socket, const PrivateCert &cert);
        void async_create_next(AsyncIo &aio, std::function<void()> handler, AsyncIo::ErrorHandler error);
    };
}
