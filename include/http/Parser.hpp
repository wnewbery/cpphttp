#pragma once
#include "Request.hpp"
#include "Response.hpp"
#include "Headers.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace http
{
    template<class IMPL, class Message>
    class Parser
    {
    public:
        enum ParserStatus
        {
            NOT_STARTED,
            READING_HEADERS,
            READING_BODY,
            READING_BODY_CHUNKED,
            READING_BODY_CHUNKED_LENGTH,
            READING_BODY_CHUNKED_TERMINATOR,
            READING_TRAILER_HEADERS,
            COMPLETED
        };

        Parser();
        void reset();

        size_t read(const uint8_t *data, size_t len);


        bool is_completed()const { return parser_status == COMPLETED; }
    protected:
        Message message;
    private:
        ParserStatus parser_status;
        /**The expected final length of body from the Content-Length header.*/
        size_t expected_body_len;
        /**Holds data that is definately part of this response (e.g. part of a header)
         * while waiting for 'read' to be called again with more data.
         */
        std::vector<uint8_t> buffer;


        /**Try to read a line from this->buffer + data.
         *
         * @param line If a line is read, this is assigned its value not including the \r\n.
         * @param consumed_len The number of bytes of data consumed. They may have been added to
         * either this->buffer or line.
         * @param data The input data.
         * @param len The length of the input data.
         * @return True if a line was read. Data may be consumed and saved in this->buffer without
         * a full line being read, with false being returned but consumed_len greater than 0.
         */
        bool read_line(std::string *line, const uint8_t **data, const uint8_t *end);
        void add_header(const std::string &line);
    };

    class RequestParser : public Parser<RequestParser, Request>
    {
    public:
        Request& request() { return message; }
    protected:
        friend class Parser<RequestParser, Request>;
        void read_first_line(const std::string &line);
    };

    class ResponseParser : public Parser<ResponseParser, Response>
    {
    public:
        Response& response() { return message; }
    protected:
        friend class Parser<ResponseParser, Response>;
        void read_first_line(const std::string &line);
    };
}
