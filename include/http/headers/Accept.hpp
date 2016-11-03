#pragma once
#include <string>
#include <vector>
#include "Error.hpp"
namespace http
{
    /**Represents a HTTP Accept request header. */
    class Accept
    {
    public:
        /**An acceptable content type.*/
        struct Type
        {
            /**Mime content type, e.g. "text" or "application".
             * May be '*' for match all if subtype is also '*'.
             */
            std::string type;
            /**Mime content sub-type, e.g. "png" or "html".
             * May be '*' for match all.
             */
            std::string subtype;
            /**Quality level. If there is multiple possible response content types, then one with a
             * higher quality according to the requests Accept header is preferred.
             */
            float quality;

            /**True if a given content type and subtype match this, taking account of wildcard accepts.*/
            bool matches(const std::string &_type, const std::string &_subtype)const
            {
                if (type == "*") return true;
                if (type != _type) return false;
                if (subtype == "*") return true;
                return this->subtype == _subtype;
            }
            /**Return true if other is a better quality match than this.
             *    - First highest quality if preferred.
             *    - Then if this is &lowast;/&lowast; and other is x/&lowast; or x/y, other is preferred.
             *    - Then if this is x/&lowast; and other is x/y, other is preferred.
             *    - Else this is preferred.
             */
            bool better_quality(const Type &other)const
            {
                if (other.quality > quality) return true;
                if (other.quality < quality) return false;

                if (type == "*" && other.type != "*") return true;
                if (subtype == "*" && other.subtype != "*") return true;

                return false;
            }
        };

        Accept() : types() {}
        /**Constructor from Accept header value.*/
        explicit Accept(const std::string &value)
            : Accept(value.c_str(), value.c_str() + value.size())
        {}
        /**Constructor from Accept header value.*/
        Accept(const char *begin, const char *end);

        /**Return true if content_type is accepted for any quality.*/
        bool accepts(const std::string &content_type)const
        {
            return find(content_type) != nullptr;
        }
        /**@throws NotAcceptable if the type is not accept.*/
        void check_accepts(const std::string &content_type)const
        {
            if (!accepts(content_type)) throw NotAcceptable({ content_type });
        }

        /**Return which of the list of types is most preferred.
         * E.g. to select between JSON and XML.
         * @throws NotAcceptable if none of the types are accepted.
         */
        std::string preferred(const std::vector<std::string> &types)const;

        /**Gets the list of acceptable content types.*/
        const std::vector<Type>& accepts()const { return types; }
    private:
        std::vector<Type> types;

        const Type *find(const std::string &type)const;
        void parse(const char *begin, const char *end);
    };
}

