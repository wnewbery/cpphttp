#include "server/Router.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "Status.hpp"
#include "Error.hpp"

namespace http
{
    Router::Router()
        : paths()
    {}

    Router::~Router()
    {}

    void Router::add(const std::string &method, const std::string &path, RequestHandlerPtr handler)
    {
        auto &x = paths[path];
        auto added = x.methods.emplace(method, handler).second;
        if (!added) throw std::runtime_error("Router already had handler for " + method + " " + path);
    }

    const RequestHandler *Router::get_handler(const std::string &method, const std::string &path)const
    {
        auto path_handler = paths.find(path);
        if (path_handler == paths.end()) throw NotFound(path);

        auto &methods = path_handler->second.methods;
        auto method_handler = methods.find(method);
        if (method_handler == methods.end()) throw MethodNotAllowed(method, path);

        return method_handler->second.get();
    }
}
