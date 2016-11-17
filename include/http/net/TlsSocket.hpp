#pragma once
#ifdef _WIN32
#include "SchannelSocket.hpp"
namespace http
{
    typedef SchannelSocket TlsSocket;
    typedef SchannelServerSocket TlsServerSocket;
}
#else
#include "OpenSslSocket.hpp"
namespace http
{
    typedef OpenSslSocket TlsSocket;
    typedef OpenSslServerSocket TlsServerSocket;
}
#endif
