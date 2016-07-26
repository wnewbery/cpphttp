#pragma once
#include <stdexcept>

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

    class NotFound : public ErrorResponse
    {
    public:
        NotFound(const std::string &path)
            : ErrorResponse("Not Found " + path, 404)
        {}
    };
    class MethodNotAllowed : public ErrorResponse
    {
    public:
        MethodNotAllowed(const std::string &method, const std::string &path)
            : ErrorResponse(method + " not allowed for " + path, 405)
        {}
    };
}
