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

        /**The suggested HTTP response status code.*/
        int status_code()const { return _status_code; }
    protected:
        int _status_code;
    };

    /**Represents a non-success HTTP response.*/
    class ErrorResponse : public std::runtime_error
    {
    public:
        /**HTTP error response with a message and status code.*/
        ErrorResponse(const std::string &msg, int status_code)
            : std::runtime_error(msg), _status_code(status_code)
        {}
        /**The HTTP status code for the response.*/
        int status_code()const { return _status_code; }
    private:
        int _status_code;
    };

    /**4xx error.*/
    class ClientErrorResponse : public ErrorResponse
    {
    public:
        using ErrorResponse::ErrorResponse;
    };

    /**400 Bad Request.*/
    class BadRequest : public ClientErrorResponse
    {
    public:
        /**Construct with a specified message body describing the error.*/
        explicit BadRequest(const std::string &message)
            : ClientErrorResponse(message, 400)
        {}
    };
    /**404 Not Found.*/
    class NotFound : public ClientErrorResponse
    {
    public:
        /**Construct a generic message for a path.*/
        explicit NotFound(const std::string &path)
            : ClientErrorResponse("Not Found " + path, 404)
        {}
    };
    /**405 Method Not Allowed.*/
    class MethodNotAllowed : public ClientErrorResponse
    {
    public:
        /**Construct a generic message for a method and path.*/
        MethodNotAllowed(const std::string &method, const std::string &path)
            : ClientErrorResponse(method + " not allowed for " + path, 405)
        {}
        /**Construct a generic message for a method and path.*/
        MethodNotAllowed(Method method, const std::string &path)
            : MethodNotAllowed(std::to_string(method), path)
        {}
    };
    /**406 Not Acceptable.*/
    class NotAcceptable : public ClientErrorResponse
    {
    public:
        /**Construct a "Not Acceptable" response with a list of content types that are acceptable.*/
        NotAcceptable(const std::vector<std::string> &accepts)
            : ClientErrorResponse("No acceptable content type", 406)
            , _accepts(accepts)
        {}
        /**Get the list of content types that would be accepted.*/
        const std::vector<std::string>& accepts()const { return _accepts; }
    private:
        std::vector<std::string> _accepts;
    };
}
