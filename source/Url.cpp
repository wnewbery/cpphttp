#include "Url.hpp"
#include <sstream>
namespace http
{
    namespace
    {
        static const std::string EMPTY_STRING;
        static const Url::QueryParamList EMPTY_LIST;

        char hex_value(char c)
        {
            if (c >= '0' && c <= '9') return (char)(c - '0');
            if (c >= 'a' && c <= 'f') return (char)(c - 'a' + 10);
            if (c >= 'A' && c <= 'F') return (char)(c - 'A' + 10);
            throw std::runtime_error("Invalid percent encoding hex value");
        }
        char hex_value(char c1, char c2)
        {
            return (char)((hex_value(c1) << 4) | hex_value(c2));
        }
        char hex_chr(char c)
        {
            if (c < 10) return (char)('0' + c);
            else return (char)('A' + c - 10);
        }
    }

    std::string url_decode(const std::string &str)
    {
        std::string out;
        out.reserve(str.size());
        for (size_t i = 0; i < str.size();)
        {
            if (str[i] == '%' && i + 2 < str.size())
            {
                out += hex_value(str[i + 1], str[i + 2]);
                i += 3;
            }
            else
            {
                out += str[i];
                ++i;
            }
        }
        return out;
    }

    std::string url_decode_query(const std::string &str)
    {
        std::string out;
        out.reserve(str.size());
        for (size_t i = 0; i < str.size();)
        {
            if (str[i] == '+')
            {
                out += ' ';
                ++i;
            }
            else if (str[i] == '%' && i + 2 < str.size())
            {
                out += hex_value(str[i + 1], str[i + 2]);
                i += 3;
            }
            else
            {
                out += str[i];
                ++i;
            }
        }
        return out;
    }

    bool uri_unreserved_chr(char c)
    {
        if (c >= 'A' && c <= 'Z') return true;
        if (c >= 'a' && c <= 'z') return true;
        if (c >= '0' && c <= '9') return true;
        return c == '-' || c == '_' || c == '.' || c == '~';
    }
    std::string url_encode_path(const std::string &str)
    {
        std::string out;
        out.reserve(str.size());
        for (auto c : str)
        {
            if (c == '/') out += '/';
            else if (uri_unreserved_chr(c)) out += c;
            else
            {
                out += '%';
                out += hex_chr((char)(c >> 4));
                out += hex_chr((char)(c & 0x0F));
            }
        }
        return out;
    }

    std::string url_encode_query(const std::string &str)
    {
        std::string out;
        out.reserve(str.size());
        for (auto c : str)
        {
            if (c == ' ') out += '+';
            else if (uri_unreserved_chr(c)) out += c;
            else
            {
                out += '%';
                out += hex_chr((char)(c >> 4));
                out += hex_chr((char)(c & 0x0F));
            }
        }
        return out;
    }

    Url Url::parse_request(const std::string &str)
    {
        Url url;
        //  /path/string[?query=param[&another=param]]
        if (str.empty() || str[0] != '/') throw UrlError(str, "Request URL must start with a '/'");

        auto query_start = str.find('?');
        if (query_start == std::string::npos)
        {
            url.path = url_decode(str);
        }
        else
        {
            url.path = url_decode(str.substr(0, query_start));
            auto i = query_start + 1;
            //split up the query string
            while (i < str.size())
            {
                //each key-value pair
                auto j = i;
                std::string key, value;
                for (;; ++j)
                {
                    if (j == str.size() || str[j] == '&')
                    {   //valueless param (e.g. "?cache&id=-5"
                        key = str.substr(i, j - i);
                        break;
                    }
                    else if (str[j] == '=')
                    {   //parse value part
                        key = str.substr(i, j - i);
                        auto k = ++j;
                        for (; k != str.size() && str[k] != '&'; ++k);
                        value = str.substr(j, k - j);
                        j = k;
                        break;
                    }
                }

                url.query_params[url_decode_query(key)].push_back(url_decode_query(value));
                i = j + 1;
            }
        }

        return url;
    }

    Url::Url()
        : protocol(), host(), port(0), path(), query_params()
    {}

    bool Url::has_query_param(const std::string &name)const
    {
        return query_params.find(name) != query_params.end();
    }

    const std::string &Url::query_param(const std::string &name)const
    {
        auto it = query_params.find(name);
        if (it != query_params.end()) return it->second.front();
        else return EMPTY_STRING;
    }

    const Url::QueryParamList& Url::query_param_list(const std::string &name)const
    {
        auto it = query_params.find(name);
        if (it != query_params.end()) return it->second;
        else return EMPTY_LIST;
    }

    void Url::encode_request(std::ostream &os)const
    {
        if (path.empty() || path[0] != '/') throw std::runtime_error("URL path must start with '/'");
        os << url_encode_path(path);
        bool first_param = true;
        for (auto &param : query_params)
        {
            for (auto &value : param.second)
            {
                if (first_param)
                {
                    os << '?';
                    first_param = false;
                }
                else os << '&';
                os << url_encode_query(param.first);
                if (!param.second.empty())
                {
                    os << '=';
                    os << url_decode_query(value);
                }
            }
        }
    }

    std::string Url::encode_request()const
    {
        std::stringstream ss;
        encode_request(ss);
        return ss.str();
    }

    void Url::encode(std::ostream &os)const
    {
        if (!protocol.empty())
        {
            os << protocol << ':';
            if (host.empty()) throw std::runtime_error("URL cant have protocol without host");
        }
        if (!host.empty())
        {
            os << "//" << host;
        }
        if (port)
        {
            if (host.empty()) throw std::runtime_error("URL can not have port without host");
            os << ':' << port;
        }
        encode_request(os);
    }

    std::string Url::encode()const
    {
        std::stringstream ss;
        encode(ss);
        return ss.str();
    }
}
