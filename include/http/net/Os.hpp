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
    typedef int SOCKET;
    static const int INVALID_SOCKET = -1;
    static const int SOCKET_ERROR = -1;
    static const int SD_RECEIVE = SHUT_RD;
    static const int SD_SEND = SHUT_WR;
    static const int SD_BOTH = SHUT_RDWR;

    inline void closesocket(SOCKET socket)
    {
        ::close(socket);
    }
}

#endif
