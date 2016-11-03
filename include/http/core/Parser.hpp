#pragma once
#include "ParserUtils.hpp"
#include "../Headers.hpp"
#include "../Status.hpp"
#include "../Method.hpp"
#include <algorithm>

namespace http
{
    /**HTTP parser base for server and client side messages.
     * Provides the base implementation for RequestParser and ResponseParser, which provide
     * some key functionality.
     */
    class BaseParser
    {
    public:
        /**Max size of any line. A buffer of this size will be capable of parsing any HTTP component.*/
        static const size_t LINE_SIZE = 8192;
        /**Max size of first line.*/
        static const size_t FIRST_LINE_LEN = LINE_SIZE;
        /**Max number of headers.*/
        static const size_t MAX_HEADER_COUNT = 100;
        /**Max size of a single header line.*/
        static const size_t MAX_HEADER_SIZE = LINE_SIZE;
        /**Max combined size of headers.*/
        static const size_t MAX_HEADERS_SIZE = 65536;
        /**Max size of a chunk length line.*/
        static const size_t MAX_CHUNK_LINE_SIZE = 10;

        enum State
        {
            /**Not parsed any content yet, awaiting completion of the first line.*/
            START,
            /**Read the first line and now reading headers.
             * Already completed headers are available to access.
             */
            HEADERS,
            /**Reading body data of a known length defined by the Content-Type header.*/
            BODY,
            /**Reading chunked encoding body data.*/
            BODY_CHUNK,
            /**Next is a chunked encoding chunk length header.*/
            BODY_CHUNK_LEN,
            /**Next is a "\r\n" chunked encoding terminator.*/
            BODY_CHUNK_TERMINATOR,
            /**Read body data until connection is closed.*/
            BODY_UNTIL_CLOSE,
            /**Trailer headers for chunked-encoding.*/
            TRAILER_HEADERS,
            /**Message parsing completed.*/
            COMPLETED
        };

        BaseParser();
        /**Reset the parser so it is ready to read another message.*/
        void reset();

        /**Reading the entire HTTP request or response message is complete.*/
        bool is_completed()const { return _state == COMPLETED; }

        /**Get the current parser progress state.*/
        State state()const { return _state; }
        /**Get the HTTP version. Only valid if progressed to HEADERS or beyond.*/
        const Version& version()const { return _version; }
        /**Get the parsed headers. The headers container may be safely moved if parsing headers
         * is complete.
         */
        Headers&& headers() { return std::move(_headers); }
        /**Get the parsed headers.*/
        const Headers& headers()const { return _headers; }
        /**Get the message body.
         * Allthough this is a std::string, it may contain binary data.
         * Can be safely moved once reading the entire body is complete.
         */
        std::string &&body() { return std::move(_body); }
        /**Get the message body.
         * Allthough this is a std::string, it may contain binary data.
         */
        const std::string &body()const { return _body; }

        /**True if the content length is known (content_length is valid), allthough the message
         * body may not have yet been read (is_completed may be false).
         */
        bool has_content_length()const
        {
            return _state == BODY || _state == COMPLETED;
        }
        /**Get the content body length. Only valid if has_content_length.*/
        size_t content_length()const
        {
            return _content_length;
        }
    protected:

        State _state;
        Headers _headers;
        Version _version;
        size_t _content_length;
        size_t remaining_content_length;
        std::string _body;

        /**Read a header line (state == HEADERS).
         * @param str The header to read.
         * @param end The end of str, does not include the trailing \r\n.
         */
        void read_header(const char *str, const char *end);
        /**Start reading the body.
         * Transfer-Encoding: chunked is supported, but no others are.
         */
        template<bool REQUEST> void start_body();
        /**Checks version is 1.x*/
        void check_version()const
        {
            if (_version.major != 1) throw ParserError("Unsupported version", 505);
        }
        template<typename IMPL> const char *do_read(const char *begin, const char *end);
        void process_body(const char *begin, const char *end)
        {
            _body.insert(_body.end(), begin, end);
        }
    };

    /**HTTP server side request parser.*/
    class RequestParser : public BaseParser
    {
    public:
        /**Read input.
         *
         * May not consume all of the input, if there is remaining input, and the request is not
         * complete then additional data must be made available, and read called against starting
         * from that point.
         *
         * If the request is complete, then the input is expected to be for another, pipelined
         * request.
         *
         * @return Pointer to 1 element past the consumed input.
         */
        const char *read(const char *begin, const char *end);

        /**Get the HTTP request method.*/
        const std::string &method()const { return _method; }
        /**get the request URI. This only includes the path onwards.
         * The host and port can be obtained from the "Host" header.
         */
        const std::string &uri()const { return _uri; }
    private:
        friend class BaseParser;

        std::string _method;
        std::string _uri;

        void read_first_line(const char *str, const char *end);
        void start_body();
    };
    /**HTTP client side response parser.*/
    class ResponseParser : public BaseParser
    {
    public:
        /**Read input. Has the same behaviour as RequestParser::read.*/
        const char *read(const char *begin, const char *end);
        /**Reset ready to read a response following a request of a specific method.
         * The method is needed for the response to know if it should expect a body or not
         * (e.g. a HEAD response will have Content-Length etc., but does not send the body bytes).
         */
        void reset(Method method);
        /**Gets the response status code and message.*/
        Status status()const { return _status; }
    private:
        friend class BaseParser;

        Method method;
        Status _status;

        void read_first_line(const char *str, const char *end);
        void start_body();
    };
}
