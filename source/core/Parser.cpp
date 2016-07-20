#include "core/Parser.hpp"
#include "core/ParserUtils.hpp"
#include <cassert>

namespace http
{
    BaseParser::BaseParser()
    {
        reset();
    }
    void BaseParser::reset()
    {
        _state = START;
        _headers.clear();
        _body.clear();
        _content_length = 0;
    }
    void BaseParser::read_header(const char *str, const char *end)
    {
        auto name_end = parser::read_header_name(str, end);
        assert(name_end < end && *name_end == ':');

        auto val_start = parser::skip_ows(name_end + 1, end);
        auto val_end = parser::read_header_value(val_start, end);
        auto line_end = parser::skip_ows(val_end, end);
        if (line_end != end) throw ParserError("Unexpected content after header value");

        _headers.add({str, name_end}, {val_start, val_end});
    }

    template<bool REQUEST>
    void BaseParser::start_body()
    {
        //RFC 7230 3.3
        assert(_state == HEADERS);
        //TODO: Abort if these headers got set multiple times. See RFC 7230 3.3.3.

        auto te = _headers.get("Transfer-Encoding");
        if (!te.empty())
        {
            if (te == "chunked")
            {
                _state = BODY_CHUNK_LEN;
            }
            else throw ParserError("Only 'chunked' transfer encoding is supported", SC_NOT_IMPLEMENTED);
        }
        else
        {
            auto len_str = _headers.get("Content-Length");
            if (!len_str.empty())
            {
                int len = -1;
                size_t count = 0;
                try { len = std::stoi(len_str, &count); }
                catch (const std::exception &) {}
                if (count != len_str.size() || len < 0)
                {
                    throw ParserError("Invalid Content-Length header value");
                }

                remaining_content_length = _content_length = (size_t)len;
                if (_content_length == 0) _state = COMPLETED;
                else _state = BODY;
            }
            else
            {
                if (REQUEST)
                {
                    _state = COMPLETED;
                }
                else
                {
                    _state = BODY_UNTIL_CLOSE;
                    throw ParserError(
                        "Responses with a body but no Transfer-Encoding or Content-Length are not supported",
                        SC_LENGTH_REQUIRED);
                }
            }
        }
    }

    template<typename IMPL>
    const char *BaseParser::do_read(const char *begin, const char *end)
    {
        assert (!is_completed());

        if (_state == START)
        {
            auto line_end = parser::find_newline(begin, end);
            if (!line_end) return begin;
            ((IMPL*)this)->read_first_line(begin, line_end);
            _state = HEADERS;
            begin = line_end + 2;
        }

        while (_state == HEADERS)
        {
            auto line_end = parser::find_newline(begin, end);
            if (!line_end) return begin;
            else if (begin == line_end)
            {
                begin += 2;
                ((IMPL*)this)->start_body();
                break;
            }
            else
            {
                read_header(begin, line_end);
                begin = line_end + 2;
            }
        }

        if (_state == BODY)
        {
            assert(remaining_content_length > 0 && remaining_content_length <= _content_length);
            auto avail = (size_t)(end - begin);
            auto consume = std::min(avail, remaining_content_length);
            process_body(begin, begin + consume);
            remaining_content_length -= consume;
            begin += consume;
            if (!remaining_content_length) _state = COMPLETED;
            return begin;
        }

        while (true)
        {
            if (_state == BODY_CHUNK)
            {
                assert(remaining_content_length > 0);
                auto avail = (size_t)(end - begin);
                auto consume = std::min(avail, remaining_content_length);
                process_body(begin, begin + consume);
                remaining_content_length -= consume;
                begin += consume;
                if (!remaining_content_length) _state = BODY_CHUNK_TERMINATOR;
            }
            else if (_state == BODY_CHUNK_LEN)
            {
                auto limit = std::min(end, begin + MAX_CHUNK_LINE_SIZE);
                auto line_end = parser::find_newline(begin, limit);
                if (!line_end)
                {
                    if (begin + MAX_CHUNK_LINE_SIZE == limit)
                        throw ParserError("Did not find chunk length CRLF within allowed length");
                    return begin;
                }

                size_t count = 0;
                unsigned long
                    len = 0;
                try { len = std::stoul(std::string{begin, line_end}, &count, 16); }
                catch (const std::exception &){}
                if (begin + count != line_end) throw ParserError("Invalid chunk size");

                begin = line_end + 2;
                remaining_content_length = len;
                _content_length += len;

                if (remaining_content_length == 0) _state = TRAILER_HEADERS;
                else _state = BODY_CHUNK;
            }
            else if (_state == BODY_CHUNK_TERMINATOR)
            {
                if (begin + 2 >= end) return begin;
                if (begin[0] == '\r' && begin[1] == '\n')
                {
                    begin += 2;
                    _state = BODY_CHUNK_LEN;
                }
                else throw ParserError("Expected CRLF after chunk data.");
            }
            else break;
        }

        while (_state == TRAILER_HEADERS)
        {
            auto line_end = parser::find_newline(begin, end);
            if (!line_end) return begin;
            else if (begin == line_end)
            {
                begin += 2;
                _state = COMPLETED;
                return begin;
            }
            else
            {
                read_header(begin, line_end);
                begin = line_end + 2;
            }
        }

        return begin;
    }

    const char *RequestParser::read(const char *begin, const char *end)
    {
        return do_read<RequestParser>(begin, end);
    }

    void RequestParser::read_first_line(const char *str, const char *end)
    {
        assert(_state == START);
        auto p = parser::read_method(str, end);
        _method.assign(str, p);

        auto uri_end = parser::read_uri(p + 1, end);
        _uri.assign(p + 1, uri_end);

        parser::read_request_version(uri_end + 1, end, &_version);
        check_version();

        _state = HEADERS;
    }

    void RequestParser::start_body()
    {
        BaseParser::start_body<true>();
    }

    const char *ResponseParser::read(const char *begin, const char *end)
    {
        return do_read<ResponseParser>(begin, end);
    }

    void ResponseParser::reset(Method _method)
    {
        method = _method;
        BaseParser::reset();
    }

    void ResponseParser::read_first_line(const char *str, const char *end)
    {
        assert(_state == START);
        parser::StatusCode sc;

        auto p = parser::read_response_version(str, end, &_version);
        check_version();
        p = parser::read_status_code(p + 1, end, &sc);
        _status.code = (StatusCode)sc.code;

        parser::read_status_phrase(p + 1, end);
        _status.msg.assign(p + 1, end);

        _state = HEADERS;
    }

    void ResponseParser::start_body()
    {
        //TODO: Need to get method from the origenal request...
        auto sc = _status.code;
        if (sc / 100 != 1 && sc != 204 && sc != 304 &&
            method != HEAD && !(method == CONNECT && sc / 100 == 2))
        {
            BaseParser::start_body<false>();
        }
        else _state = COMPLETED;
    }
}
