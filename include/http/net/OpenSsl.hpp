#pragma once
#include "Os.hpp"
#include "Net.hpp"
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
        extern SSL_CTX* openssl_ctx;
        extern const SSL_METHOD *openssl_method;
        extern SSL_CTX* openssl_server_ctx;
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
