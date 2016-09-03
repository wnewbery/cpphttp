#pragma once
#include <stdexcept>
#include <string>
#include <vector>
#include "Method.hpp"

namespace http
{
    /**@brief Errors while parsing HTTP messages.
     * If status_code is non-zero, it is a suggested HTTP error status code to send if parsing a
     * request, such as an overlong URI or unsupported method.
     */
    class ParserError : public std::runtime_error
    {
    public:
        ParserError(const std::string &msg, int status_code = 0)
            : std::runtime_error(msg), _status_code(status_code)
        {}

        int status_code()const { return _status_code; }
    protected:
        int _status_code;
    };

    class ErrorResponse : public std::runtime_error
    {
    public:
        ErrorResponse(const std::string &msg, int status_code)
            : std::runtime_error(msg), _status_code(status_code)
        {}

        int status_code()const { return _status_code; }
    protected:
        int _status_code;
    };

    /**4xx error*/
    class ClientErrorResponse : public ErrorResponse
    {
    public:
        using ErrorResponse::ErrorResponse;
    };

    /**400 Bad Request*/
    class BadRequest : public ClientErrorResponse
    {
    public:
        BadRequest(const std::string &message)
            : ClientErrorResponse(message, 400)
        {}
    };
    /**404 Not Found*/
    class NotFound : public ClientErrorResponse
    {
    public:
        NotFound(const std::string &path)
            : ClientErrorResponse("Not Found " + path, 404)
        {}
    };
    /**405 Method Not Allowed*/
    class MethodNotAllowed : public ClientErrorResponse
    {
    public:
        MethodNotAllowed(const std::string &method, const std::string &path)
            : ClientErrorResponse(method + " not allowed for " + path, 405)
        {}
        MethodNotAllowed(Method method, const std::string &path)
            : MethodNotAllowed(std::to_string(method), path)
        {}
    };
    /**406 Not Acceptable*/
    class NotAcceptable : public ClientErrorResponse
    {
    public:
        NotAcceptable(const std::vector<std::string> &accepts)
            : ClientErrorResponse("No acceptable content type", 406)
            , _accepts(accepts)
        {}
        const std::vector<std::string>& accepts()const { return _accepts; }
    private:
        std::vector<std::string> _accepts;
    };
}
