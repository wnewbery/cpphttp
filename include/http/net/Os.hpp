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

namespace http
{
    typedef int SOCKET;
    static const int INVALID_SOCKET = -1;
    static const int SOCKET_ERROR = -1;

    inline closesocket(SOCKET socket)
    {
        ::close(socket);
    }
}

#endif
