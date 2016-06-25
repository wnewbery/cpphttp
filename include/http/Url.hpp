#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <stdexcept>

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

    class Url
    {
    public:
        typedef std::unordered_multimap<std::string, std::string> QueryParams;
        class QueryParamList
        {
        public:
            class const_iterator
            {
            public:
                typedef const std::string value_type;
                typedef value_type& reference;
                typedef value_type* pointer;
                typedef QueryParams::const_iterator::difference_type difference_type;
                typedef std::forward_iterator_tag iterator_category;

                explicit const_iterator(QueryParams::const_iterator it) : it(it) {}

                const std::string& operator * ()const { return it->second; }
                const std::string* operator -> ()const { return &it->second; }

                const_iterator& operator ++ ()
                {
                    ++it;
                    return *this;
                }
                const_iterator operator ++ (int)
                {
                    auto ret = *this;
                    ++*this;
                    return ret;
                }

                friend bool operator == (const const_iterator &a, const const_iterator &b)
                {
                    return a.it == b.it;
                }
                friend bool operator != (const const_iterator &a, const const_iterator &b)
                {
                    return a.it != b.it;
                }
            private:
                QueryParams::const_iterator it;
            };

            QueryParamList(QueryParams::const_iterator _begin, QueryParams::const_iterator _end)
                : _begin(_begin), _end(_end)
            {}

            size_t size()const { return std::distance(_begin, _end); }
            const_iterator begin()const { return const_iterator(_begin); }
            const_iterator cbegin()const { return const_iterator(_begin); }
            const_iterator end()const { return const_iterator(_end); }
            const_iterator cend()const { return const_iterator(_end); }
        private:
            QueryParams::const_iterator _begin, _end;
        };

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
        const QueryParamList query_param_list(const std::string &name)const;
    };
}
