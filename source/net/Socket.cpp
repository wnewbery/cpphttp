#include "net/Socket.hpp"
#include "net/Net.hpp"
namespace http
{
    void Socket::send_all(const void *buffer, size_t len)
    {
        while (len > 0)
        {
            auto sent = send(buffer, len);
            if (sent == 0) throw SocketError("send_all failed");
            len -= sent;
            buffer = ((const char*)buffer) + sent;
        }
    }
}
