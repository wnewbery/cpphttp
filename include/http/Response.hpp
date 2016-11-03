#pragma once
#include "Headers.hpp"
#include "Status.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace http
{
    /**HTTP Response message.*/
    class Response
    {
    public:
        /**Response status code and message.*/
        Status status;
        /**Response headers.*/
        Headers headers;
        /**Response body. Allthough this is an std::string, it may also contain binary data.*/
        std::string body;

        /**Set the status code and message.*/
        void status_code(StatusCode sc)
        {
            status.code = sc;
            status.msg = default_status_msg(sc);
        }
        /**Set the status code and message.*/
        void status_code(int sc)
        {
            status_code((StatusCode)sc);
        }
    };
}
