#pragma once
#include "Headers.hpp"
#include "Status.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace http
{
    class Response
    {
    public:
        StatusCode status_code;
        std::string status_msg;

        Headers headers;
        std::vector<uint8_t> body;

        void status(StatusCode sc)
        {
            status_code = sc;
            status_msg = default_status_msg(sc);
        }
    };
}
