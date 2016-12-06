#pragma once
#include "Os.hpp"
#include "Net.hpp"
#include <memory>
#include <string>
#include <openssl/opensslconf.h>
#include <openssl/ssl.h>

#ifndef OPENSSL_THREADS
#   error OPENSSL_THREADS required
#endif
#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "libssl.lib")

namespace http
{
    namespace detail
    {
        struct OpenSslDeleter
        {
            void operator()(SSL *ssl)const
            {
                if (ssl) SSL_free(ssl);
            }
            void operator()(SSL_CTX *ctx)const
            {
                if (ctx) SSL_CTX_free(ctx);
            }
        };

        extern std::unique_ptr<SSL_CTX, OpenSslDeleter> openssl_ctx;
        extern const SSL_METHOD *openssl_method;
        extern std::unique_ptr<SSL_CTX, OpenSslDeleter> openssl_server_ctx;
        extern const SSL_METHOD *openssl_server_method;

        /**Get a descriptive error string for an OpenSSL error code.*/
        std::string openssl_err_str(SSL *ssl, int error);
        void init_openssl();
    }
    /**Errors from OpenSSL.*/
    class OpenSslSocketError : public SocketError
    {
    public:
        /**Create an error message using openssl_err_str.*/
        OpenSslSocketError(SSL *ssl, int error)
            : SocketError(detail::openssl_err_str(ssl, error))
        {
        }
        OpenSslSocketError(const std::string &msg, SSL *ssl, int error)
            : SocketError(msg + ": " + detail::openssl_err_str(ssl, error))
        {
        }
    };
}
