#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <ostream>
#include <vector>

namespace http
{
    /**Errors for invalid URL's being parsed or built.*/
    class UrlError : public std::runtime_error
    {
    public:
        /**Report a descriptive error about a specific URL string.*/
        UrlError(const std::string &url, const std::string &msg)
            : std::runtime_error(msg + ": " + url)
        {}
    };

    /**Percent decode a URL fragement.*/
    std::string url_decode(const std::string &str);
    /**Percent decode a URL query string element, additionally converting '+' to ' '.*/
    std::string url_decode_query(const std::string &str);

    /**Percent encode a URL path segment.*/
    std::string url_encode_path(const std::string &str);
    /**Percent encode a URL query string element, additionally converting ' ' to '+'.*/
    std::string url_encode_query(const std::string &str);

    /**A URL split into its components.*/
    class Url
    {
    public:
        /**Container of query parameter values for a specific named parameter.*/
        typedef std::vector<std::string> QueryParamList;
        /**Map container of query parameters.*/
        typedef std::unordered_map<std::string, QueryParamList> QueryParams;

        /**HTTP request string does not include protocol, host, port, or hash. */
        static Url parse_request(const std::string &str);
        /**Connection protocol to use. e.g. "http" or "https".*/
        std::string protocol;
        /**Host name or IP address.*/
        std::string host;
        /**Port number (e.g. 80 or 443).
         * If no port number is explicitly specified, the value is zero.
         * To get a port number accounting for default protocol ports, use port_or_default.
         */
        uint16_t port;
        /**Percent encoded URL path.*/
        std::string path;
        /**Percent decoded query parameters.
         * There may be multiple parameters with the same name. No special meaning is given to
         * such parameters.
         */
        QueryParams query_params;

        Url();

        /**True if there is at least one query parameter with a given case sensitive name.*/
        bool has_query_param(const std::string &name)const;
        /**Get a query parameter value.
         *    - If there are multiple parameters with the same name, then the first (left-most) one
         * is returned.
         *
         *      If it is required to handle multiple parameters use query_param_list.
         *    - If there is no parameter with the name, then an empty string is returned.
         *
         *      If it is required to tell existance or valuelessness apart, use has_query_param
         *      or query_param_list.
         */
        const std::string &query_param(const std::string &name)const;
        /**Gets the list of query parameters with a given name in order (left to right in the URL).
         * If there are no parameters with the name, then an empty list is returned.
         */
        const QueryParamList& query_param_list(const std::string &name)const;

        /**Encodes the path onwards for use in a HTTP request message.*/
        std::string encode_request()const;
        /**Write encode_request to the stream.*/
        void encode_request(std::ostream &os)const;

        /**Write the full URL string to a stream.*/
        void encode(std::ostream &os)const;
        /**Get the full URL string.*/
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
