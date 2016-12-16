#pragma once
#if !defined(_WIN32) && !defined(HTTP_USE_OPENSSL)
    #define HTTP_USE_OPENSSL
#endif
#if !defined(HTTP_USE_SELECT) && !defined(HTTP_USE_IOCP)
    #ifdef _WIN32
        #define HTTP_USE_IOCP
    #else
        #define HTTP_USE_SELECT
    #endif
#endif

#ifdef _WIN32

#ifndef NOMINMAX
#   define NOMINMAX
#endif
#ifndef SECURITY_WIN32
#   define SECURITY_WIN32
#endif
#if !defined(WIN32_MEAN_AND_LEAN) && !defined(HTTP_NO_WIN32_MEAN_AND_LEAN)
#   define WIN32_MEAN_AND_LEAN
#endif

#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <Windows.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Secur32.lib")
#pragma comment(lib, "Mswsock.lib")

namespace http
{
    inline SOCKET create_socket(int af = AF_INET, int type = SOCK_STREAM, int prot = IPPROTO_TCP)
    {
        return WSASocket(af, type, prot, nullptr, 0, WSA_FLAG_OVERLAPPED);
    }
}
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

    inline SOCKET create_socket(int af = AF_INET, int type = SOCK_STREAM, int prot = IPPROTO_TCP)
    {
        return ::socket(af, type, prot);
    }
    //On Windows file descriptors and sockets are different. But on Linux all sockets are file descriptors.
    inline void closesocket(SOCKET socket)
    {
        ::close(socket);
    }
}

#endif
