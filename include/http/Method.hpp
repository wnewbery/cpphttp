#pragma once
#include <string>
#ifdef DELETE
#undef DELETE
#endif
namespace http
{
    /**Known HTTP methods enumeration.*/
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
