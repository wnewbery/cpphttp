#include "core/ParserUtils.hpp"
namespace http
{
    namespace parser
    {
        namespace
        {
            bool is_digit(char c)
            {
                return c >= '0' && c <= '9';
            }
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

            /**Read until end of token, does not validate if the end octet is allowed or not.*/
            const char *read_token(const char *begin, const char *end)
            {
                auto p = begin;
                while (p < end && is_token_chr(*p)) ++p;
                return p;
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
            for (auto p = begin + 1; p < end; ++p)
            {
                if (is_ows(*p)) return p;
                else if (!is_field_vchar(*p)) throw ParserError("Invalid octet in header field value");
            }
            return end;
        }

        const char *skip_ows(const char *begin, const char *end)
        {
            while (begin < end && is_ows(*begin)) ++begin;
            return begin;
        }
    }
}
