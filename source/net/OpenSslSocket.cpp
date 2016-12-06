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

        ssl.reset(SSL_new(openssl_ctx.get()));

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

    void OpenSslSocket::async_disconnect(AsyncIo &,
        std::function<void()> handler, AsyncIo::ErrorHandler error)
    {
        try
        {
            assert(SSL_is_init_finished(ssl.get()));
            assert(BIO_ctrl_pending(in_bio) == 0 && BIO_ctrl_pending(out_bio) == 0);
            disconnect();
            handler();
        }
        catch (const std::exception&) { error(); }
    }
    void OpenSslSocket::async_recv(AsyncIo &aio, void *buffer, size_t len,
        AsyncIo::RecvHandler handler, AsyncIo::ErrorHandler error)
    {
        // Try to read data either buffered by SSL or by in_bio synchronously.
        // Read asynchronously from underlying socket and retry if get SSL_ERROR_WANT_READ.
        try
        {
            assert(BIO_ctrl_pending(out_bio) == 0);
            if (len > (size_t)std::numeric_limits<int>::max())
                len = (size_t)std::numeric_limits<int>::max();

            auto len2 = SSL_read(ssl.get(), buffer, (int)len);
            if (len2 > 0)
            {
                return handler((size_t)len2);
            }
            else
            {
                auto err = SSL_get_error(ssl.get(), len2);
                if (len2 == 0 && err == SSL_RECEIVED_SHUTDOWN)
                {
                    return handler(0);
                }
                else if (len2 < 0 && err == SSL_ERROR_WANT_READ)
                {
                    return tcp.async_recv(aio, bio_buffer, sizeof(bio_buffer),
                        [this, &aio, buffer, len, handler, error](size_t recv_len)
                        {
                            if (recv_len == 0) handler(0);
                            else
                            {
                                BIO_write(in_bio, bio_buffer, (int)recv_len);
                                async_recv(aio, buffer, len, handler, error);
                            }
                        }, error);
                }
                else throw OpenSslSocketError("SSL_read failed", ssl.get(), len2);
            }
        }
        catch (const std::exception &) { error(); }
    }
    void OpenSslSocket::async_send(AsyncIo &aio, const void *buffer, size_t len,
        AsyncIo::SendHandler handler, AsyncIo::ErrorHandler error)
    {
        async_send_all(aio, buffer, len, handler, error);
    }
    void OpenSslSocket::async_send_all(AsyncIo &aio, const void *buffer, size_t len,
        AsyncIo::SendHandler handler, AsyncIo::ErrorHandler error)
    {
        assert(len > 0);
        async_send_next(aio, buffer, len, 0, handler, error);
    }
    void OpenSslSocket::async_send_next(AsyncIo &aio, const void *buffer, size_t len, size_t sent,
        AsyncIo::SendHandler handler, AsyncIo::ErrorHandler error)
    {
        assert(SSL_is_init_finished(ssl.get()));
        assert(BIO_ctrl_pending(out_bio) == 0);
        assert(len <= (size_t)std::numeric_limits<int>::max());
        try
        {
            auto ret = SSL_write(ssl.get(), buffer, (int)len);
            if (ret <= 0) throw OpenSslSocketError(ssl.get(), ret);

            sent += ret;
            buffer = ((const char*)buffer) + ret;
            assert(sent <= len);

            async_send_bio(aio,
                [this, &aio, buffer, len, sent, handler, error]()
                {
                    if (sent == len) handler(len);
                    else async_send_next(aio, buffer, len, sent, handler, error);
                }, error);
        }
        catch (const std::exception &) { error(); }
    }
    void OpenSslSocket::async_send_bio(AsyncIo &aio, std::function<void()> handler, AsyncIo::ErrorHandler error)
    {
        assert(BIO_ctrl_pending(out_bio) > 0);
        auto len = BIO_read(out_bio, bio_buffer, (int)sizeof(bio_buffer));
        if (len <= 0) throw std::runtime_error("BIO_read failed");
        tcp.async_send_all(aio, bio_buffer, (size_t)len,
            [this, &aio, handler, error](size_t)
            {
                if (BIO_ctrl_pending(out_bio) > 0) async_send_bio(aio, handler, error);
                else handler();
            },
            error);
    }

    OpenSslServerSocket::OpenSslServerSocket(TcpSocket &&socket, const PrivateCert &cert)
        : OpenSslSocket()
    {
        setup(std::move(socket), cert);
        SSL_set_fd(ssl.get(), (int)tcp.get());
        if (SSL_accept(ssl.get()) <= 0)
            throw std::runtime_error("SSL_accept failed");
    }

    void OpenSslServerSocket::async_create(AsyncIo &aio, TcpSocket &&socket, const PrivateCert &cert,
        std::function<void()> handler, AsyncIo::ErrorHandler error)
    {
        setup(std::move(socket), cert);

        in_bio = BIO_new(BIO_s_mem());
        out_bio = BIO_new(BIO_s_mem());

        BIO_set_mem_eof_return(in_bio, EOF);
        BIO_set_mem_eof_return(out_bio, EOF);

        SSL_set_bio(ssl.get(), in_bio, out_bio);

        SSL_set_accept_state(ssl.get());

        async_create_next(aio, handler, error);
    }

    void OpenSslServerSocket::setup(TcpSocket &&socket, const PrivateCert &cert)
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
    }

    void OpenSslServerSocket::async_create_next(AsyncIo &aio, std::function<void()> handler, AsyncIo::ErrorHandler error)
    {
        try
        {
            assert(!SSL_is_init_finished(ssl.get()));
            // Check next step
            auto ret = SSL_do_handshake(ssl.get());
            if (ret < 0)
            {
                auto err = SSL_get_error(ssl.get(), ret);
                if (err == SSL_ERROR_WANT_WRITE || BIO_ctrl_pending(out_bio))
                {
                    return async_send_bio(aio, std::bind(&OpenSslServerSocket::async_create_next, this, std::ref(aio), handler, error), error);
                }
                else if (err == SSL_ERROR_WANT_READ)
                {
                    tcp.async_recv(aio, bio_buffer, sizeof(bio_buffer),
                        [this, &aio, handler, error](size_t len)
                        {
                            if (len == 0)
                            {
                                try { throw ConnectionError("Client disconnected before TLS handshake complete", tcp.host(), tcp.port()); }
                                catch (const std::exception &) { error(); }
                            }
                            BIO_write(in_bio, bio_buffer, (int)len);
                            async_create_next(aio, handler, error);
                        }, error);
                    return;
                }
                else
                {
                    throw OpenSslSocketError("Handshake failure", ssl.get(), ret);
                }
            }
            else if (ret == 1)
            {
                assert(SSL_is_init_finished(ssl.get()));
                if (BIO_ctrl_pending(out_bio)) return async_send_bio(aio, handler, error);
                else return handler();
            }
            else throw std::runtime_error("Unexpected SSL_do_handshake result");
        }
        catch (const std::exception &)
        {
            return error();
        }
    }
}
