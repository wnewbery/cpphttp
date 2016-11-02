#pragma once
#include "../Request.hpp"
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
namespace http
{
    class Response;
    class UrlError;
    typedef std::unordered_map<std::string, std::string> PathParams;
    class RequestHandler
    {
    public:
        virtual ~RequestHandler() {}

        virtual Response handle(Request &request, PathParams &path_params)=0;
    };
    typedef std::shared_ptr<RequestHandler> RequestHandlerPtr;
    /**A route found by Router for a path and method.
     * If true, contains the handler and path parameters.
     */
    struct MatchedRoute
    {
        /**Request handler if a route was matched, else null.*/
        const RequestHandler *handler;
        /**Named path parameters from the matched URL path.*/
        PathParams path_params;

        MatchedRoute() : handler(nullptr), path_params() {}
        /**True if a route was found.*/
        explicit operator bool()const { return handler != nullptr; }
    };

    /**Thrown when trying to add a route that is invalid.*/
    class InvalidRouteError : public std::runtime_error
    {
    public:
        InvalidRouteError(const std::string &method, const std::string &path, const std::string &reason)
            : std::runtime_error("Invalid route " + method + " " + path + ": " + reason)
        {}
    };

    /**Finds handlers for requests by method and uri path.*/
    class Router
    {
    public:
        Router() {}
        ~Router() {}

        /**Gets the handler for a request that was added to this router.
         * @throws MethodNotAllowed if a handler matched the path, but not the method.
         * @return The found route, or a null handler if no route was found.
         */
        MatchedRoute get(const std::string &method, const std::string &path)const;
        /**Adds a handler for a method and path.
         *
         * Paths and parameter names are case sensitive, and each segment is URL percent decoded.
         *
         * Path segments starting with a colon (':') are path parameters, and the text following
         * the colon up to the next forward slash is the parameter name.
         *
         * If the final segment is "/*", then this is a prefix route, and will match all child
         * paths.
         *
         * Example Paths:
         *    - "/" Site root page
         *    - "/assets/*" All files/items under assets, e.g. "/assets/application.js".
         *    - "/profiles/:profile_id" A specific profile, with "profile_id" as a path parameter.
         *
         * @throws InvalidRouteError
         *    - If a path parameter name does not match existing routes.
         *    - If the method and path combination has already been added.
         *    - If the path already exists as a prefix, and this one is not.
         *    - If adding a prefix path, and the path already exists as a non-prefix path.
         */
        void add(const std::string &method, const std::string &path, RequestHandlerPtr handler);
    private:
        typedef std::vector<std::string> PathParts;
        typedef PathParts::const_iterator PathIterator;
        /**A node representing a single path segment.*/
        struct Node
        {
            struct Param
            {
                std::string name;
                std::unique_ptr<Node> node;

                explicit operator bool()const
                {
                    return (bool)node;
                };
            };
            /**Prefix node for e.g. /assets/*. Otherwise need to match all segments.
             * A prefix node can not have child nodes.
             */
            bool prefix;
            std::unordered_map<std::string, RequestHandlerPtr> methods;
            /**Named child paths.*/
            std::unordered_map<std::string, Node> children;
            /**Parameter child node. Note that all such routes must use a common parameter name.*/
            Param param;

            Node() : prefix(false), methods(), children(), param() {}
        };
        /**Root of the paths tree ('/').*/
        Node root;
        /**Splits a URL path by forward slashes into percent-decoded parts.
         * The path is expected to begin with a forward slash, and the empty initial part is not included.
         *
         * e.g. "/profiles/55" will return `{"profiles", "55"}`.
         */
        PathParts get_parts(const std::string &path)const;
    };
}
