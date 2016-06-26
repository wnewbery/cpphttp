#include "Method.hpp"
#include <stdexcept>
#include <unordered_map>
namespace http
{
    namespace
    {
        const std::unordered_map<std::string, Method> STR_METHOD =
        {
            { "GET", GET },
            { "HEAD", HEAD },
            { "POST", POST },
            { "PUT", PUT },
            { "DELETE", DELETE },
            { "TRACE", TRACE },
            { "OPTIONS", OPTIONS },
            { "CONNECT", CONNECT },
            { "PATCH", PATCH }
        };
    }
    Method method_from_string(const std::string &str)
    {
        auto it = STR_METHOD.find(str);
        if (it != STR_METHOD.end()) return it->second;
        else throw std::runtime_error("Invalid HTTP method " + str);
    }
    
    std::string to_string(Method method)
    {
        switch (method)
        {
        case GET: return "GET";
        case HEAD: return "HEAD";
        case POST: return "POST";
        case PUT: return "PUT";
        case DELETE: return "DELETE";
        case TRACE: return "TRACE";
        case OPTIONS: return "OPTIONS";
        case CONNECT: return "CONNECT";
        case PATCH: return "PATCH";
        default: std::terminate();
        }
    }
}
