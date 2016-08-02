#include "core/ParserUtils.hpp"
namespace http
{
    namespace parser
    {
        namespace
        {
            bool is_alpha(char c)
            {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
            }
            /**RFC 7230 Appendix B.*/
            bool is_token_chr(char c)
            {
                if (is_digit(c) || is_alpha(c)) return true;
                switch (c)
                {
                case '!':
                case '#':
                case '$':
                case '%':
                case '&':
                case '\'':
                case '*':
                case '+':
                case '-':
                case '.':
                case '^':
                case '_':
                case '`':
                case '|':
                case '~':
                    return true;
                default:
                    return false;
                }
            }
            /**https://en.wikipedia.org/wiki/Augmented_Backus%E2%80%93Naur_Form#Core_rules*/
            bool is_vchar(char c)
            {
                return c >= 0x21 && c <= 0x7E;
            }
            /**RFC 7230 Appendix B.*/
            bool is_obs_text(char c)
            {
                return ((unsigned char)c) >= 0x80;
            }
            bool is_reason_phrase_char(char c)
            {
                if (is_vchar(c)) return true;
                if (c == '\t' || c == ' ') return true;
                return false;
            }
            bool is_field_vchar(char c)
            {
                return is_vchar(c) || is_obs_text(c);
            }
            bool is_ows(char c)
            {
                return c == ' ' || c == '\t';
            }
            bool is_qdtext(char c)
            {
                return c == '\t' || c == ' ' || c == '!' ||
                    (c >= 0x23 && c <= 0x5B) ||
                    (c >= 0x5D && c <= 0x7E) ||
                    is_obs_text(c);
            }

            /**Read a decimal number.*/
            const char *read_decimal(const char *begin, const char *end, int *out)
            {
                int x = 0;
                auto p = begin;
                for (; p < end && is_digit(*p); ++p)
                {
                    x = x * 10 + (*p - '0');
                }
                if (p == begin) throw ParserError("Expected decimal number");
                *out = x;
                return p;
            }
        }
        const char *find_newline(const char *begin, const char *end)
        {
            for (auto p = begin; p < end - 1; ++p)
            {
                if (p[0] == '\r' && p[1] == '\n')
                {
                    return p;
                }
            }
            return nullptr;
        }

        const char *read_token(const char *begin, const char *end)
        {
            auto p = begin;
            while (p < end && is_token_chr(*p)) ++p;
            if (p == begin) throw ParserError("Expected token");
            return p;
        }

        const char *read_list_sep(const char *begin, const char *end)
        {
            // 1#element => *( "," OWS ) element *( OWS "," [ OWS element ]) a
            auto p = begin;
            p = skip_ows(p, end);
            if (p == end) return end;
            if (*p != ',') throw ParserError("Expected ',' in comma-delimited list");

            while (true) //skip empty elements
            {
                p = skip_ows(p + 1, end);

                if (p == end) return end;
                else if (*p == ',') continue;
                else return p;
            }
        }

        const char *read_qstring(const char *begin, const char *end, std::string *out)
        {
            if (begin == end || *begin != '"') throw ParserError("Expected \"");
            out->clear();
            auto p = begin + 1;
            while (p < end)
            {
                if (*p == '"') return p + 1; //end delimiter
                else if (*p == '\\')
                {
                    if (p + 1 == end) throw ParserError("Unexpected end in quoted-string");
                    if (*p == '\t') *out += '\t';
                    else if (*p == ' ') *out += ' ';
                    else if (is_vchar(*p)) *out += *p;
                    else if (is_obs_text(*p)) *out += *p;
                    else throw ParserError("Invalid quoted-pair escape in quoted-string");
                    p += 2;
                }
                else if (is_qdtext(*p))
                {
                    *out += *p;
                    ++p;
                }
                else throw ParserError("Invalid octet in quoted-string");
            }
            throw ParserError("Expected \" to end quoted-string");
        }

        const char *read_token_or_qstring(const char *begin, const char *end, std::string *out)
        {
            if (begin == end) throw ParserError("Unexpected end, expected token or quoted-string");
            if (*begin == '"') return read_qstring(begin, end, out);
            else
            {
                auto p = read_token(begin, end);
                out->assign(begin, p);
                return p;
            }
        }

        const char *read_method(const char *begin, const char *end)
        {
            auto ret = read_token(begin, end);
            if (ret == end || *ret != ' ') throw ParserError("Expected SP after request method.");
            return ret;
        }
        const char *read_uri(const char *begin, const char *end)
        {
            //request-target in RFC 7230 5.3
            if (begin == end || *begin == ' ') throw ParserError("Request URI can not be empty.");
            for (auto p = begin + 1; p < end; ++p)
            {
                if (*p == ' ') return p;
                else if (*p <= 0x1F || *p == 0x7F) throw ParserError("Control codes not allowed in URI.");
            }
            throw ParserError("Expected SP after request URI.");
        }
        const char *read_version(const char *begin, const char *end, Version *out)
        {
            if (begin + 5 > end) throw ParserError("Missing HTTP version");
            if (begin[0] != 'H' || begin[1] != 'T' || begin[2] != 'T' || begin[3] != 'P' || begin[4] != '/')
                throw ParserError("Expected HTTP version");
            auto p = begin + 5;

            p = read_decimal(p, end, &out->major);
            if (p >= end || p[0] != '.') throw ParserError("Expected '.' between HTTP major and minor version");
            p = read_decimal(p + 1, end, &out->minor);
            
            return p;
        }
        const char *read_request_version(const char *begin, const char *end, Version *out)
        {
            auto p = read_version(begin, end, out);
            if (p != end) throw ParserError("Expected CRLF after request version");
            return p;
        }
        const char *read_response_version(const char *begin, const char *end, Version *out)
        {
            auto p = read_version(begin, end, out);
            if (p >= end || p[0] != ' ') throw ParserError("Expected SP after response version");
            return p;
        }
        const char *read_status_code(const char *begin, const char *end, StatusCode *out)
        {
            auto p = read_decimal(begin, end, &out->code);
            
            if (p < end && p[0] == '.') //IIS extended code
            {
                p = read_decimal(p + 1, end, &out->extended);
            }
            else out->extended = -1;
            
            if (p >= end || p[0] != ' ') throw ParserError("Expected SP after status code");
            return p;
        }
        void read_status_phrase(const char *begin, const char *end)
        {
            for (auto p = begin; p < end; ++p)
            {
                if (!is_reason_phrase_char(*p)) throw ParserError("Invalid octect in response reason phrase");
            }
        }

        const char *read_header_name(const char *begin, const char *end)
        {
            auto p = read_token(begin, end);
            //Note: Do not try to allow whitespace here, even for compatibility, RFC 7230 3.2.4
            //"A server MUST reject any received request message that contains whitespace between
            //a header field - name and colon"
            if (p >= end || p[0] != ':') throw ParserError("Expected ':' after header name");
            return p;
        }

        const char *read_header_value(const char *begin, const char *end)
        {
            if (begin == end || is_ows(*begin)) throw ParserError("Header values can not be empty");
            auto last_non_ws = begin;
            for (auto p = begin + 1; p < end; ++p)
            {
                if (is_ows(*p)) continue;
                else if (is_field_vchar(*p)) last_non_ws = p;
                else throw ParserError("Invalid octet in header field value");
            }
            return last_non_ws + 1;
        }

        const char *skip_ows(const char *begin, const char *end)
        {
            while (begin < end && is_ows(*begin)) ++begin;
            return begin;
        }
    }
}
