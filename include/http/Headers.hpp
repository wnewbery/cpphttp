#pragma once
#include <string>
#include <unordered_map>
namespace http
{
    struct ContentType
    {
        std::string mime;
        std::string charset;
    };

    class Headers
    {
    public:
        typedef std::unordered_map<std::string, std::string> Container;
        typedef Container::iterator iterator;
        typedef Container::const_iterator const_iterator;

        iterator begin() { return data.begin(); }
        const_iterator begin()const { return data.begin(); }
        const_iterator cbegin()const { return data.cbegin(); }
        iterator end() { return data.end(); }
        const_iterator end()const { return data.end(); }
        const_iterator cend()const { return data.cend(); }

        void clear() { data.clear(); }

        void add(const std::string &key, const std::string &value)
        {
            //TODO: Deal with multiple headers with same name
            data[key] = value;
        }
        void set(const std::string &key, const std::string &value)
        {
            //TODO: Error if already have multiple headers with same name
            data[key] = value;
        }
        bool has(const std::string &key)const
        {
            return data.count(key) > 0;
        }
        const std::string& get(const std::string &key)const;
        const_iterator find(const std::string &key)const
        {
            return data.find(key);
        }

        ContentType content_type()const;
        void content_type(const std::string &mime, const std::string &charset = std::string())
        {
            if (charset.empty()) add("Content-Type", mime);
            else add("Content-Type", mime + "; charset=" + charset);
        }
        void content_type(const ContentType &type)
        {
            content_type(type.mime, type.charset);
        }

        void remove(const std::string &key)
        {
            data.erase(key);
        }

        size_t size()const { return data.size(); }
    private:
        Container data;
    };
    typedef Headers ResponseHeaders;
    typedef Headers RequestHeaders;
}
