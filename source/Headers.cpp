#include "Headers.hpp"
namespace http
{
    namespace
    {
        static const std::string EMPTY_STRING;
    }

    const std::string& Headers::get(const std::string &key)const
    {
        auto it = data.find(key);
        if (it != data.end()) return it->second;
        else return EMPTY_STRING;
    }

    ContentType Headers::content_type()const
    {
        auto val = get("Content-Type");
        if (val.empty()) return {};

        auto sep = val.find(';');
        if (sep == std::string::npos) return { val, std::string() };

        auto mime = val.substr(0, sep);
        auto charset_p = val.find("; charset=");
        auto charset = val.substr(charset_p + sizeof("; charset=") - 1);

        return { mime, charset };
    }
}
