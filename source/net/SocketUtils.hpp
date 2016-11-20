#pragma once
#include "net/Os.hpp"
#include "net/Net.hpp"
#include <cassert>
#include <fcntl.h>
namespace http
{
    inline void set_non_blocking(SOCKET sock, bool non_blocking)
    {
        assert(sock != INVALID_SOCKET);
#ifdef WIN32
        unsigned long mode = non_blocking ? 0 : 1;
        auto ret = ioctlsocket(sock, FIONBIO, &mode);
#else
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags < 0) throw std::runtime_error("fcntl get failed");
        flags = non_blocking ? (flags&~O_NONBLOCK) : (flags | O_NONBLOCK);
        auto ret = fcntl(sock, F_SETFL, flags);
#endif
        if (ret) throw SocketError(last_net_error());
    }
}
