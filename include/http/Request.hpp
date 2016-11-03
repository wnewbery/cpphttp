#pragma once
#include "Method.hpp"
#include "Headers.hpp"
#include "Url.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace http
{
    /**HTTP Request message.*/
    class Request
    {
    public:
        /**HTTP request method.*/
        Method method;
        /**Raw HTTP url.
         * When sending a request, if this is non-empty, it is used in preference, else
         * "url.encode_request()" is used.
         */
        std::string raw_url;
        /**URL object. See also raw_url.*/
        Url url;

        /**HTTP headers.*/
        Headers headers;
        /**Request body. Allthough this is an std::string, it may also contain binary data.*/
        std::string body;
    };
}
