#pragma once
#ifdef _WIN32
#include "SchannelListenSocket.hpp"
namespace http
{
    typedef SchannelListenSocket TlsListenSocket;
}
#else
#include "OpenSslListenSocket.hpp"
namespace http
{
    typedef OpenSslListenSocket TlsListenSocket;
}
#endif
