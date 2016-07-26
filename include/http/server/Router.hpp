#pragma once
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
namespace http
{
    class Request;
    class Response;
    class RequestHandler
    {
    public:
        virtual ~RequestHandler() {}

        virtual Response handle(Request &request)=0;
    };
    typedef std::shared_ptr<RequestHandler> RequestHandlerPtr;
    
    /**A path in the router. Contains handlers for each method.
     */
    class RouterPath
    {
    public:
        typedef std::unordered_map<std::string, RequestHandlerPtr> Methods;
        std::string path;
        Methods methods;
    };

    /**Finds handlers for requests by method and uri path.
     * Throws NotFound and MethodNotAllowed exceptions.
     */
    class Router
    {
    public:
        Router();
        ~Router();

        void add(const std::string &method, const std::string &path, RequestHandlerPtr handler);
        /**Get the handler for a request.
         * @throws NotFound if no handler matched the specified path.
         * @throws MethodNotAllowed if a handler matched the path, but not the method.
         */
        const RequestHandler *get_handler(const std::string &method, const std::string &path)const;
    private:
        typedef std::unordered_map<std::string, RouterPath> Paths;

        Paths paths;
    };
}
