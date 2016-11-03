#pragma once
#include <string>
#include <unordered_map>
namespace http
{
    /**Simple representation of the HTTP Content-Type header.*/
    struct ContentType
    {
        /**Mime type for the content type.*/
        std::string mime;
        /**Charset for the content type.*/
        std::string charset;
    };

    /**Container of HTTP request or response headers.*/
    class Headers
    {
    public:
        typedef std::unordered_map<std::string, std::string> Container;
        /**An unordered forward iterator.*/
        typedef Container::iterator iterator;
        /**An unordered forward iterator.*/
        typedef Container::const_iterator const_iterator;

        /**Begin iterator.*/
        iterator begin() { return data.begin(); }
        /**Begin iterator.*/
        const_iterator begin()const { return data.begin(); }
        /**Begin iterator.*/
        const_iterator cbegin()const { return data.cbegin(); }
        /**End iterator.*/
        iterator end() { return data.end(); }
        /**End iterator.*/
        const_iterator end()const { return data.end(); }
        /**End iterator.*/
        const_iterator cend()const { return data.cend(); }

        /**Deletes all headers.*/
        void clear() { data.clear(); }

        /**Add a new header that is known to not already exist.*/
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
        /**Set, if not already set.*/
        void set_default(const std::string &key, const std::string &value)
        {
            auto &v = data[key];
            if (v.empty()) v = value;
        }

        /**True if there is a header by this name.*/
        bool has(const std::string &key)const
        {
            return data.count(key) > 0;
        }
        /**Get the value of a header. An empty string is returned if the header is not present.*/
        const std::string& get(const std::string &key)const;
        /**Find a header as an iterator.*/
        const_iterator find(const std::string &key)const
        {
            return data.find(key);
        }

        /**Get the Content-Type header.*/
        ContentType content_type()const;
        /**Set the Content-Type header.*/
        void content_type(const std::string &mime, const std::string &charset = std::string())
        {
            if (charset.empty()) add("Content-Type", mime);
            else add("Content-Type", mime + "; charset=" + charset);
        }
        /**Set the Content-Type header.*/
        void content_type(const ContentType &type)
        {
            content_type(type.mime, type.charset);
        }

        /**Remove a header if it is present.*/
        void remove(const std::string &key)
        {
            data.erase(key);
        }

        /**Get the number of headers.*/
        size_t size()const { return data.size(); }
    private:
        Container data;
    };
    typedef Headers ResponseHeaders;
    typedef Headers RequestHeaders;
}
