#pragma once
#include "Os.hpp"

#include <schannel.h>
#include <security.h>

namespace http
{
    namespace detail
    {
        extern SecurityFunctionTableW *sspi; //http::init_net, Net.cpp

        struct SecBufferSingleAutoFree
        {
            SecBuffer buffer;
            SecBufferDesc desc;

            SecBufferSingleAutoFree()
                : buffer{ 0, SECBUFFER_TOKEN, nullptr }
                , desc{ SECBUFFER_VERSION, 1, &buffer }
            {
            }
            ~SecBufferSingleAutoFree()
            {
                if (buffer.pvBuffer) sspi->FreeContextBuffer(buffer.pvBuffer);
            }
        };
    }
}
