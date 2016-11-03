#pragma once
#ifdef _WIN32
#define NOMINMAX
#define SECURITY_WIN32
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <Windows.h>
#include <schannel.h>
#include <security.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Secur32.lib")
#else

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

namespace http
{
    //WinSock provides some useful types and constants that Linux does not
    typedef int SOCKET;
    static const int INVALID_SOCKET = -1;
    static const int SOCKET_ERROR = -1;
    //WinSock and Linux use different names for the `shutdown` constants
    static const int SD_RECEIVE = SHUT_RD;
    static const int SD_SEND = SHUT_WR;
    static const int SD_BOTH = SHUT_RDWR;

    //On Windows file descriptors and sockets are different. But on Linux all sockets are file descriptors.
    inline void closesocket(SOCKET socket)
    {
        ::close(socket);
    }
}

#endif
