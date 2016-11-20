#include "net/OpenSslSocket.hpp"

#include <openssl/opensslconf.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#ifndef OPENSSL_THREADS
#   error OPENSSL_THREADS required
#endif

namespace http
{
    //http::init_net, Net.cpp
    extern SSL_CTX* openssl_ctx;
    extern const SSL_METHOD *openssl_method;
    extern const SSL_METHOD *openssl_server_method;

    std::string openssl_err_str(SSL *ssl, int error)
    {
        auto e2 = errno;
        auto serr = SSL_get_error(ssl, error);
        if (serr == SSL_ERROR_SYSCALL) return errno_string(e2);
        // Get OpenSSL error
        auto str = ERR_error_string(serr, nullptr);
        if (str) return str;
        else return errno_string(error);
    }
    
    void OpenSslSocket::SslDeleter::operator()(SSL *ssl)const
    {
        SSL_free(ssl);
    }

    OpenSslSocket::OpenSslSocket() : tcp(), ssl(nullptr)
    {
    }
    OpenSslSocket::OpenSslSocket(const std::string &host, uint16_t port)
        : tcp(), ssl(nullptr)
    {
        connect(host, port);
    }
    OpenSslSocket::~OpenSslSocket()
    {
    }

    void OpenSslSocket::connect(const std::string &host, uint16_t port)
    {
        tcp.connect(host, port);

        ssl.reset(SSL_new(openssl_ctx));

        //Currently only supporting blocking sockets, so dont care about renegotiation details
        SSL_set_mode(ssl.get(), SSL_MODE_AUTO_RETRY);

        SSL_set_fd(ssl.get(), tcp.get_socket());
        auto err = SSL_connect(ssl.get());
        if (err < 0) throw OpenSslSocketError(ssl.get(), err);
    }

    std::string OpenSslSocket::address_str()const
    {
        return tcp.address_str();
    }
    bool OpenSslSocket::check_recv_disconnect()
    {
        return tcp.check_recv_disconnect();
    }
    void OpenSslSocket::close()
    {
        tcp.close();
    }
    void OpenSslSocket::disconnect()
    {
        if (ssl)
        {
            if (SSL_shutdown(ssl.get()) == 0)
            {
                SSL_shutdown(ssl.get()); // Wait for other party
            }
        }
        tcp.disconnect();
    }
    bool OpenSslSocket::recv_pending()const
    {
        return SSL_pending(ssl.get()) > 0;
    }
    size_t OpenSslSocket::recv(void *buffer, size_t len)
    {
        auto len2 = SSL_read(ssl.get(), (char*)buffer, (int)len);
        if (len2 < 0) throw OpenSslSocketError(ssl.get(), len2);
        return (size_t)len2;
    }
    size_t OpenSslSocket::send(const void *buffer, size_t len)
    {
        auto len2 = SSL_write(ssl.get(), (const char*)buffer, (int)len);
        if (len2 < 0) throw OpenSslSocketError(ssl.get(), len2);
        return (size_t)len2;
    }

    OpenSslServerSocket::OpenSslServerSocket(TcpSocket &&socket, const std::string &cert_hostname)
        : OpenSslSocket()
    {
        openssl_ctx.reset(SSL_CTX_new(openssl_server_method));
        if (!openssl_ctx) throw std::runtime_error("SSL_CTX_new failed");

        SSL_CTX_set_ecdh_auto(openssl_ctx.get(), 1);
        if (SSL_CTX_use_certificate_file(openssl_ctx.get(), (cert_hostname + ".crt").c_str(), SSL_FILETYPE_PEM) < 0)
            throw std::runtime_error("Failed to use " + cert_hostname + ".crt");

        if (SSL_CTX_use_PrivateKey_file(openssl_ctx.get(), (cert_hostname + ".key").c_str(), SSL_FILETYPE_PEM) < 0)
            throw std::runtime_error("Failed to use " + cert_hostname + ".key");
        
        tcp = std::move(socket);

        ssl.reset(SSL_new(openssl_ctx.get()));
        SSL_set_fd(ssl.get(), tcp.get_socket());

        if (SSL_accept(ssl.get()) <= 0)
            throw std::runtime_error("SSL_accept failed");
    }
}
