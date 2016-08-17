#pragma once
#include "ParserUtils.hpp"
#include "../Headers.hpp"
#include "../Status.hpp"
#include "../Method.hpp"
#include <algorithm>

namespace http
{
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
            START,
            HEADERS,
            BODY,
            BODY_CHUNK,
            BODY_CHUNK_LEN,
            BODY_CHUNK_TERMINATOR,
            /**Read body data until connection is closed.*/
            BODY_UNTIL_CLOSE,
            TRAILER_HEADERS,
            COMPLETED
        };

        BaseParser();
        void reset();

        bool is_completed()const { return _state == COMPLETED; }

        State state()const { return _state; }
        const Version& version()const { return _version; }
        Headers&& headers() { return std::move(_headers); }
        const Headers& headers()const { return _headers; }
        std::string &&body() { return std::move(_body); }
        const std::string &body()const { return _body; }

        bool has_content_length()const
        {
            return _state == BODY || _state == COMPLETED;
        }
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

        const std::string &method()const { return _method; }
        const std::string &uri()const { return _uri; }
    private:
        friend class BaseParser;

        std::string _method;
        std::string _uri;

        void read_first_line(const char *str, const char *end);
        void start_body();
    };
    class ResponseParser : public BaseParser
    {
    public:
        const char *read(const char *begin, const char *end);
        void reset(Method method);

        Status status()const { return _status; }
    private:
        friend class BaseParser;

        Method method;
        Status _status;

        void read_first_line(const char *str, const char *end);
        void start_body();
    };
}
