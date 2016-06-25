#pragma once
#ifdef _WIN32
#include "SchannelSocket.hpp"
namespace http
{
    typedef SchannelSocket TlsSocket;
}
#else
#include "OpenSslSocket.hpp"
namespace http
{
    typedef OpenSslSocket TlsSocket;
}
#endif
