#include "server/Router.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "Status.hpp"
#include "Error.hpp"
#include "Url.hpp"

namespace http
{
    Router::PathParts Router::get_parts(const std::string &path)const
    {
        if (path.empty() || path[0] != '/') throw UrlError(path, "Expect URL path to begin with '/'");

        PathParts parts;
        for(size_t i = 1, next = 1; next < path.size(); i = next + 1)
        {
            next = path.find('/', i);
            if (next == i) continue; // Remove adjacent '/'
            else parts.push_back(url_decode(path.substr(i, next - i)));
        }
        return parts;
    }

    MatchedRoute Router::get(const std::string &method, const std::string &path)const
    {
        auto parts = get_parts(path);
        // Walk path segment tree
        MatchedRoute route;
        auto node = &root;
        for (auto i = parts.begin(); i != parts.end(); ++i)
        {
            // If found a prefix node that handles all (e.g. /assets/*), finish
            if (node->prefix) break;
            // Named segments take priority over path parameters
            auto child = node->children.find(*i);
            if (child != node->children.end())
            {
                node = child->second.get();
            }
            else if (node->param)
            {
                route.path_params[node->param.name] = *i;
                node = node->param.node.get();
            }
            else return MatchedRoute(); // Not found
        }
        // Find method
        auto handler = node->methods.find(method);
        if (handler == node->methods.end()) throw MethodNotAllowed(method, path);
        route.handler = handler->second.get();

        return route;
    }

    void Router::add(const std::string &method, const std::string &path, RequestHandlerPtr handler)
    {
        auto parts = get_parts(path);
        bool prefix;
        if (!parts.empty() && parts.back() == "*")
        {
            parts.pop_back();
            prefix = true;
        }
        else prefix = false;
        // Walk path segment tree
        auto node = &root;
        for (auto i = parts.begin(); i != parts.end(); ++i)
        {
            // If found a prefix node that handles all, and this is not a prefix part ('*' at end), its invalid
            if (node->prefix)
            {
                if (prefix) break;
                else throw InvalidRouteError(method, path, "Path already used as a prefix");
            }
            bool param = !i->empty() && i->front() == ':';
            if (param)
            {
                auto param_name = i->substr(1);
                if (node->param && node->param.name != param_name)
                    throw InvalidRouteError(method, path, "Differing route parameter names for :" + param_name);
                if (!node->param)
                {
                    node->param.name = param_name;
                    node->param.node.reset(new Node());
                }
                node = node->param.node.get();
            }
            else // Path segment
            {
              auto &p = node->children[*i];
              if (!p) p.reset(new Node());
              node = p.get();
            }
        }
        // Make node a prefix if needed
        if (prefix)
        {
            if (!node->children.empty() || node->param)
                throw InvalidRouteError(method, path, "Cant add as prefix because already has children");
            node->prefix = true;
        }
        // Add the method handler to node
        if (!node->methods.emplace(method, handler).second)
        {
            throw InvalidRouteError(method, path, "Route already exists");
        }
    }
}
