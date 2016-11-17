#pragma once
#include "net/Os.hpp"
#include "net/Net.hpp"
#include <cassert>
namespace http
{
    inline void set_non_blocking(SOCKET sock, bool non_blocking)
    {
        assert(sock != INVALID_SOCKET);
#ifdef WIN32
        unsigned long mode = non_blocking ? 0 : 1;
        auto ret = ioctlsocket(sock, FIONBIO, &mode);
#else
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0) return false;
        flags = non_blocking ? (flags&~O_NONBLOCK) : (flags | O_NONBLOCK);
        auto ret = fcntl(fd, F_SETFL, flags);
#endif
        if (ret) throw SocketError(last_net_error());
    }
}
