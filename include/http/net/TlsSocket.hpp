#pragma once
#include "Os.hpp"
#ifdef HTTP_USE_OPENSSL
#include "OpenSslSocket.hpp"
namespace http
{
    typedef OpenSslSocket TlsSocket;
    typedef OpenSslServerSocket TlsServerSocket;
}
#else
#include "SchannelSocket.hpp"
namespace http
{
    typedef SchannelSocket TlsSocket;
    typedef SchannelServerSocket TlsServerSocket;
}
#endif
