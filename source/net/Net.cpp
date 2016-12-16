#include "net/Net.hpp"
#include "net/Os.hpp"

#include "String.hpp"
#include <cassert>

#ifdef HTTP_USE_OPENSSL
#include "net/OpenSsl.hpp"
#endif

#ifdef _WIN32
#include "net/Schannel.hpp"
namespace http
{
    namespace detail
    {
        SecurityFunctionTableW *sspi;
    }

    void init_net()
    {
        WSADATA wsa_data;
        WSAStartup(MAKEWORD(2, 2), &wsa_data);
#ifdef HTTP_USE_OPENSSL
        detail::init_openssl();
#else
        detail::sspi = InitSecurityInterfaceW();
#endif
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
    bool would_block(int err)
    {
        return err == WSAEWOULDBLOCK;
    }
    std::string errno_string(int err)
    {
        char buffer[1024];
        if (strerror_s(buffer, sizeof(buffer), err))
            throw std::runtime_error("strerror_s failed");
        return buffer;
    }
    WinError::WinError(const char *msg)
        : WinError(msg, GetLastError())
    {}
}
#else
#include <cstring> //strerror_r
#include <vector>
#include <pthread.h>
#include <signal.h>
#include <iostream>
#include <cstdlib>

namespace http
{

    void init_net()
    {
        detail::init_openssl();
        //Linux sends a sigpipe when a socket or pipe has an error. better to handle the error where it
        //happens at the read/write site
        signal(SIGPIPE, SIG_IGN);
    }
    int last_net_error()
    {
        return errno;
    }
    bool would_block(int err)
    {
        return err == EAGAIN || err == EWOULDBLOCK;
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

