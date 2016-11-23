#pragma once
#include <string>
namespace http
{
    inline bool ieq(const std::string &a, const std::string &b)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
        {
            char ca = a[i], cb = b[i];
            if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
            if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');
            if (ca != cb) return false;
        }
        return true;
    }
}
#ifdef _WIN32
#include <codecvt>
namespace http
{
    inline std::string utf16_to_utf8(const std::wstring &utf16)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> convert;
        return convert.to_bytes(utf16);
    }
    inline std::wstring utf8_to_utf16(const std::string &utf8)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> convert;
        return convert.from_bytes(utf8);
    }
}
#endif
