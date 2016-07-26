#pragma once
#include "Method.hpp"
#include "Headers.hpp"
#include "Url.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace http
{
    class Request
    {
    public:
        Method method;
        /**Raw HTTP url.
         * When sending a request, if this is non-empty, it is used in preference, else
         * "url.encode_request()" is used.
         */
        std::string raw_url;
        Url url;

        Headers headers;
        std::string body;
    };
}
