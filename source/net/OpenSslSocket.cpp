#include "net/OpenSslSocket.hpp"
#include "net/OpenSslCert.hpp"
#include "net/OpenSsl.hpp"
#include "net/Cert.hpp"

#include <openssl/opensslconf.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#ifndef OPENSSL_THREADS
#   error OPENSSL_THREADS required
#endif

#include <iostream>
#include <cassert>

namespace http
{
    using namespace detail;

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

        assert(tcp.get() <= (SOCKET)INT_MAX);
        SSL_set_fd(ssl.get(), (int)tcp.get());
        auto err = SSL_connect(ssl.get());
        if (err < 0) throw OpenSslSocketError(ssl.get(), err);

        X509* cert = SSL_get_peer_certificate(ssl.get());
        if(cert) { X509_free(cert); }
        if(!cert) throw std::runtime_error(host + " did not send a TLS certificate");

        if (SSL_get_verify_result(ssl.get()) != X509_V_OK)
            throw CertificateVerificationError(host, port);
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

    OpenSslServerSocket::OpenSslServerSocket(TcpSocket &&socket, const PrivateCert &cert)
        : OpenSslSocket()
    {
        openssl_ctx.reset(SSL_CTX_new(openssl_server_method));
        if (!openssl_ctx) throw std::runtime_error("SSL_CTX_new failed");

        SSL_CTX_set_ecdh_auto(openssl_ctx.get(), 1);

        if (SSL_CTX_use_certificate(openssl_ctx.get(), cert.get()->cert) != 1)
            throw std::runtime_error("SSL_CTX_use_certificate failed");
        if (cert.get()->ca)
            if (SSL_CTX_set0_chain(openssl_ctx.get(), cert.get()->ca) != 1)
                throw std::runtime_error("SSL_CTX_set0_chain failed");

        if (SSL_CTX_use_PrivateKey(openssl_ctx.get(), cert.get()->pkey) != 1)
            throw std::runtime_error("SSL_CTX_use_PrivateKey failed");

        tcp = std::move(socket);

        ssl.reset(SSL_new(openssl_ctx.get()));
        assert(tcp.get() < INT_MAX);
        SSL_set_fd(ssl.get(), (int)tcp.get());

        if (SSL_accept(ssl.get()) <= 0)
            throw std::runtime_error("SSL_accept failed");
    }
}
