#pragma once
#include <stdexcept>
#include "../Version.hpp"
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

    /**Low level HTTP parser components.
     * Mostly a set of functions to parse each specific part of the HTTP protocol.
     */
    namespace parser
    {
        /**Finds the end of the line (CRLF), or null if the newline is not found.*/
        const char *find_newline(const char *begin, const char *end);
        /**Reads the HTTP method from the request line.
         * @return The end of the method (SP).
         * @throws ParserError An invalid octet, or end is encountered.
         */
        const char *read_method(const char *begin, const char *end);
        /**Reads the URI from the request line.
         * Note that the URI is not fully validated, only that it is ASCII, not zero length, and
         * does not contain any control codes.
         * 
         * request-target in RFC 7230 5.3
         * 
         * @return The end of the uri (SP).
         * @throws ParserError An invalid octet, or end is encountered.
         */
        const char *read_uri(const char *begin, const char *end);
        /**Reads the HTTP version for either the request or response line.
         * This must be in the form "HTTP/a.b", multiple digits are allowed.
         * @return The end of the version, generally either SP or CRLF, but not validated.
         * @throws ParserError An invalid octet, or end is encountered.
         */
        const char *read_version(const char *begin, const char *end, Version *out);
        /**read_version and validates end was reached.*/
        const char *read_request_version(const char *begin, const char *end, Version *out);
        /**read_version and validates ended on SP.*/
        const char *read_response_version(const char *begin, const char *end, Version *out);
        /**Status code read by read_status_code.*/
        struct StatusCode
        {
            /**HTTP status code.*/
            int code;
            /**Extended code used by IIS, or negative.*/
            int extended;
        };
        /**Read the status code. For IIS compatibility this does not follow
         * RFC7230, and instead both 3 digit, and 3.2 status codes are allowed.
         * @return The end of the code (SP).
         * @throws ParserError An invalid octet, or end is encountered.
         */
        const char *read_status_code(const char *begin, const char *end, StatusCode *out);
        /**Read the status phrase. Since the phrase is plain text and must be at the end, this
         * effectively is only validation.
         * @throws ParserError An invalid octet before end was reached.
         */
        void read_status_phrase(const char *begin, const char *end);

        
        /**Read the header name.
         * @return The ':' after the name.
         * @throws ParserError An invalid octet was found.
         */
        const char *read_header_name(const char *begin, const char *end);
        /**Read to the end of the header value. Does not include OWS before or after.
         * @return The first SP or HT, or end.
         * @throws ParserError An invalid octet was found.
         */
        const char *read_header_value(const char *begin, const char *end);
        /**@return The next non SP or HT element, or end.*/
        const char *skip_ows(const char *begin, const char *end);
    }
}
