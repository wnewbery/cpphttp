#pragma once
#include <string>
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
