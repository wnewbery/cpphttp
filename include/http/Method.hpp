#pragma once
#include <string>
namespace http
{
    enum Method
    {
        GET,
        HEAD,
        POST,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATCH,

        LAST_METHOD
    };

    Method method_from_string(const std::string &str);
    std::string to_string(Method method);
}
