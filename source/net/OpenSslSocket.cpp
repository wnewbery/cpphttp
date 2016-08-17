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
        if (tcp.get_socket() != INVALID_SOCKET) disconnect();
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
    void OpenSslSocket::disconnect()
    {
        if (ssl) SSL_shutdown(ssl.get());
        tcp.disconnect();
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
}
