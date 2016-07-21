#include "net/Net.hpp"
#include "net/Os.hpp"

#include "String.hpp"
#include <cassert>

#ifdef _WIN32
namespace http
{
    SecurityFunctionTableW *sspi;

    void init_net()
    {
        WSADATA wsa_data;
        WSAStartup(MAKEWORD(2, 2), &wsa_data);

        sspi = InitSecurityInterfaceW();
    }
    std::string win_error_string(int err)
    {
        struct Buffer
        {
            wchar_t *p;
            Buffer() : p(nullptr) {}
            ~Buffer()
            {
                LocalFree(p);
            }
        };
        Buffer buffer;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&buffer.p, 0, nullptr);
        return utf16_to_utf8(buffer.p);
    }
    int last_net_error()
    {
        return WSAGetLastError();
    }
    std::string errno_string(int err)
    {
        char buffer[1024];
        if (!strerror_s(buffer, sizeof(buffer), err)) throw std::runtime_error("strerror_s failed");
        return buffer;
    }
}
#else
#include <cstring> //strerror_r
#include <openssl/opensslconf.h>
#include <openssl/ssl.h>
#ifndef OPENSSL_THREADS
#   error OPENSSL_THREADS required
#endif
#include <vector>
#include <pthread.h>
#include <signal.h>

namespace http
{
    SSL_CTX* openssl_ctx;
    const SSL_METHOD *openssl_method;
    std::vector<pthread_mutex_t> openssl_mutexes;
    static unsigned long openssl_thread_id(void)
    {
        return (unsigned long)pthread_self();
    }
    void openssl_locking_callback(int mode, int type, const char *file, int line)
    {
        assert(!openssl_mutexes.empty());
        assert(type >= 0);
        assert(type < (int)openssl_mutexes.size());
        if (mode & CRYPTO_LOCK) {
            pthread_mutex_lock(&(openssl_mutexes[type]));
        }
        else {
            pthread_mutex_unlock(&(openssl_mutexes[type]));
        }
    }

    void init_net()
    {
        SSL_load_error_strings(); //OpenSSL SSL error strings

        OpenSSL_add_ssl_algorithms();
        openssl_method = SSLv23_client_method();//TLS_client_method();
        openssl_ctx = SSL_CTX_new(openssl_method);
        if (!openssl_ctx) throw std::runtime_error("SSL_CTX_new failed");

        openssl_mutexes.resize(CRYPTO_num_locks(), PTHREAD_MUTEX_INITIALIZER);
        CRYPTO_set_id_callback(openssl_thread_id);
        CRYPTO_set_locking_callback(openssl_locking_callback);

        //Linux sends a sigpipe when a socket or pipe has an error. better to handle the error where it
        //happens at the read/write site
        signal(SIGPIPE, SIG_IGN);
    }
    int last_net_error()
    {
        return errno;
    }
    std::string errno_string(int err)
    {
        char buffer[1024];
        errno = 0;
        auto ret = strerror_r(err, buffer, sizeof(buffer));
        if (errno) throw std::runtime_error("strerror_r failed");
        return ret;
    }
}
#endif

