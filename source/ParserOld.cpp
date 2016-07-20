#include "Parser.hpp"
#include <cassert>
#include "..\include\http\core\Parser.hpp"
namespace http
{
    template<class IMPL, class Message>
    Parser2<IMPL,Message>::Parser2()
    {
        reset();
    }
    template<class IMPL, class Message>
    void Parser2<IMPL, Message>::reset()
    {
        parser_status = NOT_STARTED;

        message.headers.clear();
        message.body.clear();

        expected_body_len = 0;
        buffer.clear();
    }

    template<class IMPL, class Message>
    size_t Parser2<IMPL, Message>::read(const uint8_t * data, size_t len)
    {
        assert(len > 0);
        std::string line;
        auto p = data, end = data + len;

        while (p != end)
        {
            if (parser_status == NOT_STARTED && read_line(&line, &p, end))
            {
                assert(buffer.empty());
                ((IMPL*)this)->read_first_line(line);
                parser_status = READING_HEADERS;
            }

            if (parser_status == READING_HEADERS && read_line(&line, &p, end))
            {
                assert(buffer.empty());
                if (line.empty())
                {
                    //end of headers
                    auto transfer_encoding = message.headers.find("Transfer-Encoding");
                    if (transfer_encoding != message.headers.end())
                    {
                        if (transfer_encoding->second == "chunked")
                        {
                            expected_body_len = 0;
                            parser_status = READING_BODY_CHUNKED_LENGTH;
                        }
                        else throw std::runtime_error("Unknown Transfer-Encoding " + transfer_encoding->second);
                    }
                    else
                    {
                        auto it = message.headers.find("Content-Length");
                        if (it != message.headers.end())
                        {
                            try
                            {
                                size_t count;
                                expected_body_len = (StatusCode)std::stoul(it->second, &count);
                                if (count != it->second.size()) throw std::invalid_argument("Non-numeric trailing text");
                            }
                            catch (const std::invalid_argument &)
                            {
                                throw std::runtime_error("Invalid Content-Length");
                            }
                            parser_status = READING_BODY;
                        }
                        else
                        {
                            parser_status = COMPLETED;
                        }
                    }
                }
                else add_header(line);
            }

            if (parser_status == READING_BODY || parser_status == READING_BODY_CHUNKED)
            {
                assert(!buffer.size());
                assert(expected_body_len > 0);
                auto remaining = expected_body_len - message.body.size();
                if ((size_t)(end - p) >= remaining)
                {
                    message.body.insert(message.body.end(), p, p + remaining);
                    p += remaining;
                    parser_status = parser_status == READING_BODY ? COMPLETED : READING_BODY_CHUNKED_TERMINATOR;
                }
                else
                {
                    message.body.insert(message.body.end(), p, end);
                    p = end;
                }
            }

            if (parser_status == READING_BODY_CHUNKED_TERMINATOR && read_line(&line, &p, end))
            {
                if (line.empty()) parser_status = READING_BODY_CHUNKED_LENGTH;
                else throw std::runtime_error("Expected chunk \\r\\n terminator");
            }

            if (parser_status == READING_BODY_CHUNKED_LENGTH && read_line(&line, &p, end))
            {
                try
                {
                    size_t count;
                    auto chunk_len = std::stoul(line, &count, 16);
                    if (count != line.size()) throw std::invalid_argument("Non-numeric trailing text");
                    if (chunk_len)
                    {
                        parser_status = READING_BODY_CHUNKED;
                        expected_body_len += chunk_len;
                    }
                    else parser_status = READING_TRAILER_HEADERS;
                }
                catch (const std::invalid_argument &)
                {
                    throw std::runtime_error("Invalid chunked transfer chunk length");
                }
            }

            if (parser_status == READING_TRAILER_HEADERS && read_line(&line, &p, end))
            {
                assert(buffer.empty());
                if (line.empty()) parser_status = COMPLETED; //end of headers
                else add_header(line);
            }
        }

        assert(p > data && p <= end);
        assert(p == end || is_completed());
        return p - data;
    }

    template<class IMPL, class Message>
    bool Parser2<IMPL, Message>::read_line(std::string * line, const uint8_t **pdata, const uint8_t *end)
    {
        auto data = *pdata;
        if (data == end) return false;

        //The last byte of buffer might be a \r, and the first byte of data might be a \n
        if (buffer.size() && buffer.back() == '\r' && data[0] == '\n')
        {
            line->assign(buffer.data(), buffer.data() + buffer.size() - 1); //assign, but exclude the \r
            buffer.clear();
            (*pdata) += 1; //1 for the \n
            return true;
        }

        auto p = data;
        while (true)
        {
            //looking for \r\n, so search for \r first and ignore the last byte
            auto p2 = (const uint8_t*)memchr(p, '\r', end - p - 1);
            if (p2)
            {
                if (p2[1] == '\n')
                {
                    //found end of string
                    line->assign(buffer.data(), buffer.data() + buffer.size());
                    line->insert(line->end(), data, p2);
                    *pdata = p2 + 2;
                    buffer.clear();
                    return true;
                }
                else
                {
                    //just a \r by itself, look for the next one
                    p = p2 + 1;
                }
            }
            else break; //reached end of buffer
        }
        //not found, so all of data must be part of the line
        buffer.insert(buffer.end(), data, end);
        *pdata = end;
        return false;
    }

    template<class IMPL, class Message>
    void Parser2<IMPL, Message>::add_header(const std::string &line)
    {
        auto colon = line.find(':', 0);
        if (colon == std::string::npos) throw std::runtime_error("Invalid header line");
        auto first_val = line.find_first_not_of(' ', colon + 1);
        std::string name = line.substr(0, colon);
        std::string value = line.substr(first_val);
        message.headers.add(name, value);
    }

    void RequestParser2::read_first_line(const std::string &line)
    {
        //e.g. 'GET /index.html HTTP/1.1'
        auto method_end = line.find(' ');
        auto url_end = line.find_last_of(' ');

        auto method_str = line.substr(0, method_end);
        message.method = method_from_string(method_str);

        message.raw_url = line.substr(method_end + 1, url_end - method_end - 1);
        message.url = Url::parse_request(message.raw_url);

        //TODO: Version
    }

    void ResponseParser2::read_first_line(const std::string &line)
    {
        //e.g. 'HTTP/1.1 200 OK'
        auto ver_end = line.find(' ', 0);
        auto code_end = line.find(' ', ver_end + 1);

        std::string code_str = line.substr(ver_end + 1, code_end - ver_end - 1);
        try
        {
            size_t count;
            message.status_code = (StatusCode)std::stoi(code_str, &count);
            if (count != code_str.size()) throw std::invalid_argument("Non-numeric trailing text");
        }
        catch (const std::invalid_argument &)
        {
            throw std::runtime_error("Non-numeric status code");
        }
        message.status_msg = line.substr(code_end + 1);

        //TODO: Version
    }

    template class Parser2<RequestParser2, Request>;
    template class Parser2<ResponseParser2, Response>;
}
