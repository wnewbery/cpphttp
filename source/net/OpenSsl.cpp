#define _CRT_SECURE_NO_DEPRECATE
#include "net/OpenSsl.hpp"
#include "net/Net.hpp"
#include <cassert>
#include <memory>
#include <mutex>
#include <string>
#include <openssl/err.h>
#ifdef _WIN32
#include <openssl/applink.c>
#endif
namespace http
{
    namespace detail
    {
        std::unique_ptr<std::mutex[]> openssl_mutexes;
        std::unique_ptr<SSL_CTX, OpenSslDeleter> openssl_ctx;
        const SSL_METHOD *openssl_method;
        std::unique_ptr<SSL_CTX, OpenSslDeleter> openssl_server_ctx;
        const SSL_METHOD *openssl_server_method;

        std::string openssl_err_str(SSL *ssl, int ret)
        {
            auto e2 = errno;
            auto serr = SSL_get_error(ssl, ret);
            if (serr == SSL_ERROR_SYSCALL) return errno_string(e2);
            // Get OpenSSL error
            auto str = ERR_error_string(serr, nullptr);
            if (str) return str;
            else return errno_string(ret);
        }

        unsigned long openssl_thread_id(void)
        {
#ifdef _WIN32
            return GetCurrentThreadId();
#else
            return (unsigned long)pthread_self();
#endif
        }
        void openssl_locking_callback(int mode, int type, const char * /*file*/, int /*line*/)
        {
            assert(openssl_mutexes);
            assert(type >= 0);
            assert(type < (int)CRYPTO_num_locks());
            if (mode & CRYPTO_LOCK)
            {
                openssl_mutexes[type].lock();
            }
            else
            {
                openssl_mutexes[type].unlock();
            }
        }

        void init_openssl()
        {
#ifdef _WIN32
            OPENSSL_Applink();
#endif
            SSL_load_error_strings(); //OpenSSL SSL error strings
            ERR_load_BIO_strings();
            OpenSSL_add_ssl_algorithms();

            // Client
            openssl_method = SSLv23_client_method();//TLS_client_method();
            openssl_ctx.reset(SSL_CTX_new(openssl_method));
            if (!openssl_ctx) throw std::runtime_error("SSL_CTX_new failed");
            // Load default trust roots
            if (!SSL_CTX_set_default_verify_paths(openssl_ctx.get()))
                throw std::runtime_error("SSL_CTX_set_default_verify_paths failed");
            // Server
            openssl_server_method = SSLv23_server_method();
            openssl_server_ctx.reset(SSL_CTX_new(openssl_server_method));
            if (!openssl_server_ctx) throw std::runtime_error("SSL_CTX_new failed");
            // Multi-threading
            openssl_mutexes.reset(new std::mutex[CRYPTO_num_locks()]);
            CRYPTO_set_id_callback(openssl_thread_id);
            CRYPTO_set_locking_callback(openssl_locking_callback);
        }
    }
}
