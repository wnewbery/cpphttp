#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <ostream>
#include <vector>

namespace http
{
    class UrlError : public std::runtime_error
    {
    public:
        UrlError(const std::string &url, const std::string &msg)
            : std::runtime_error(msg + ": " + url)
        {}
    };

    std::string url_decode(const std::string &str);
    std::string url_decode_query(const std::string &str);

    std::string url_encode_path(const std::string &str);
    std::string url_encode_query(const std::string &str);

    /**A URL split into its components.*/
    class Url
    {
    public:
        typedef std::vector<std::string> QueryParamList;
        typedef std::unordered_map<std::string, QueryParamList> QueryParams;

        /**HTTP request string does not include protocol, host, port, or hash. */
        static Url parse_request(const std::string &str);

        std::string protocol;
        std::string host;
        uint16_t port;
        std::string path;
        QueryParams query_params;

        Url();

        bool has_query_param(const std::string &name)const;
        const std::string &query_param(const std::string &name)const;
        const QueryParamList& query_param_list(const std::string &name)const;

        /**Encodes the path onwards.*/
        void encode_request(std::ostream &os)const;
        std::string encode_request()const;

        void encode(std::ostream &os)const;
        std::string encode()const;

        /**If port is non-zero, else return the default port for protocol if known, else throw.*/
        uint16_t port_or_default()const
        {
            if (port) return port;
            if (protocol == "http") return 80;
            if (protocol == "https") return 443;
            throw std::runtime_error("No known default port for " + protocol);
        }
    };

    inline std::string to_string(const Url &url) { return url.encode(); }
    inline std::ostream& operator << (std::ostream &os, const Url &url)
    {
        url.encode(os);
        return os;
    }
}
