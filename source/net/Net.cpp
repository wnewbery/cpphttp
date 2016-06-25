#include "net/Net.hpp"
#include "net/Os.hpp"

#include "String.hpp"

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
namespace
{
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

