#include "headers/Accept.hpp"
#include "core/ParserUtils.hpp"
namespace http
{
    Accept::Accept(const char *begin, const char *end)
        : types()
    {
        parse(begin, end);
    }

    std::string Accept::preferred(const std::vector<std::string> &_types)const
    {
        const Type *best = nullptr;
        const std::string *best_mime = nullptr;
        for (auto &type : _types)
        {
            auto accepted = find(type);
            if (accepted)
            {
                if (!best_mime || best->better_quality(*accepted))
                {
                    best = accepted;
                    best_mime = &type;
                }
            }
        }
        if (best_mime) return *best_mime;
        else throw NotAcceptable(_types);
    }

    const Accept::Type *Accept::find(const std::string &type)const
    {
        const Type *best = nullptr;
        auto sep = type.find('/');
        if (sep == std::string::npos)
            throw std::invalid_argument("Invalid mime type: " + type);
        auto group = type.substr(0, sep);
        auto subtype = type.substr(sep + 1);
        for (auto &x : types)
        {
            if (x.matches(group, subtype))
            {
                //for equal quality x/y overrides x/* which overrides */* and then left to right
                if (!best || best->better_quality(x))
                {
                    best = &x;
                }
            }
        }
        return best;
    }

    void Accept::parse(const char *begin, const char *end)
    {
        // RFC7231
        // Accept = #( media-range [ accept-params ] )
        // media-range = ("*/*"
        //    / (type "/" "*")
        //    / (type "/" subtype)
        //    ) *(OWS ";" OWS parameter)
        // accept-params = weight *(accept-ext)
        // accept-ext = OWS ";" OWS token["=" (token / quoted-string)]
        // type = token
        // subtype = token
        // weight = OWS ";" OWS "q=" qvalue
        // qvalue = ( "0" [ "." 0*3DIGIT ] ) / ( "1" [ "." 0*3("0") ] )
        // parameter = token "=" ( token / quoted-string )
        //
        // NOTE: weight ("q=") must take priority over parameter.
        // parameter is part of the media type, but q= and onwards are HTTP.
        auto p = begin;
        while (p < end)
        {
            Type type;
            type.quality = 1; //default
            //type
            if (*p == '*') //"*/*"
            {
                if (end - begin < 3) throw ParserError("Invalid Accept header");
                if (p[1] != '/' || p[2] != '*') throw ParserError("Invalid Accept header");
                p += 3;
                type.type = '*';
                type.subtype = '*';
            }
            else
            {
                auto p2 = parser::read_token(p, end);
                if (p2 == end || *p2 != '/') throw ParserError("Invalid Accept header");
                type.type.assign(p, p2);

                p = p2 + 1;
                if (p == end) throw ParserError("Invalid Accept header");
                if (*p == '*')
                {
                    type.subtype = '*';
                    ++p;
                }
                else
                {
                    p2 = parser::read_token(p, end);
                    type.subtype.assign(p, p2);
                    p = p2;
                }
            }
            //params
            while (true)
            {
                p = parser::skip_ows(p, end);
                if (p == end || *p == ',') break;
                else if (*p != ';' || ++p == end) throw ParserError("Invalid Accept header");

                if (end - p > 2 && p[0] == 'q' && p[1] == '=')
                {
                    //end of media type params. quality, then accept params.
                    p += 2;
                    if (p == end) throw ParserError("Expected quality value");
                    if (*p == '0') //0 plus 3 optional decimals
                    {
                        auto p2 = p;
                        type.quality = 0;
                        if (++p2 != end && *p2 == '.')
                        {
                            if (++p2 != end && parser::is_digit(*p2))
                            {
                                type.quality += (*p2 - '0') * 0.1f;
                                if (++p2 != end && parser::is_digit(*p2))
                                {
                                    type.quality += (*p2 - '0') * 0.01f;
                                    if (++p2 != end && parser::is_digit(*p2))
                                    {
                                        type.quality += (*p2 - '0') * 0.001f;
                                        ++p2;
                                    }
                                }
                            }
                        }

                        p = p2;
                    }
                    else if (*p == '1') //1 plus 3 optional zeros
                    {
                        type.quality = 1;
                        if (++p != end && *p == '.')
                        {
                            if (++p != end && *p == '0')
                            {
                                if (++p != end && *p == '0')
                                {
                                    if (++p != end && *p == '0')
                                    {
                                        ++p;
                                    }
                                }
                            }
                        }
                    }
                    else throw ParserError("Invalid quality value");

                    while (true)
                    {
                        p = parser::skip_ows(p, end);
                        if (p == end || *p == ',') break;
                        else if (*p != ';' || ++p == end) throw ParserError("Invalid Accept header");

                        p = parser::skip_ows(p, end);
                        std::string name, value;
                        auto p2 = parser::read_token(p, end);
                        if (p2 == end || *p2 != '=') throw ParserError("Invalid media type accept-ext name");
                        name.assign(p, p2);
                        p = parser::read_token_or_qstring(p2 + 1, end, &value);
                        // TODO: Store param
                    }
                    break;
                }
                else
                {
                    std::string name, value;
                    auto p2 = parser::read_token(p, end);
                    if (p2 == end || *p2 != '=') throw ParserError("Invalid media type parameter name");
                    name.assign(p, p2);
                    p = parser::read_token_or_qstring(p2 + 1, end, &value);
                    // TODO: Store param
                }
            }
            types.push_back(std::move(type));
            p = parser::read_list_sep(p, end);
        }
    }
}
