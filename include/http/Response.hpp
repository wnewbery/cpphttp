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
        Status status;

        Headers headers;
        std::string body;

        void status_code(StatusCode sc)
        {
            status.code = sc;
            status.msg = default_status_msg(sc);
        }
    };
}
