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
        std::string raw_url;
        Url url;

        Headers headers;
        std::vector<uint8_t> body;
    };
}
