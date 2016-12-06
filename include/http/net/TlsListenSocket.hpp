#pragma once
#include "Os.hpp"
#ifdef HTTP_USE_OPENSSL
#include "OpenSslListenSocket.hpp"
namespace http
{
    typedef OpenSslListenSocket TlsListenSocket;
}
#else
#include "SchannelListenSocket.hpp"
namespace http
{
    typedef SchannelListenSocket TlsListenSocket;
}
#endif
