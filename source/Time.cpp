#include "Time.hpp"
#include <sstream>
#include <stdexcept>

namespace http
{
    std::string format_time(time_t utc)
    {
        char buffer[sizeof("DDD, DD MMM YYYY HH:MM:SS GMT")];
        tm tm;
#ifdef _MSC_VER
        gmtime_s(&tm, &utc);
#else
        gmtime_r(&utc, &tm);
#endif
        size_t len = strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        return {buffer, len};
    }

    namespace
    {
        time_t mkgmtime(tm *tm)
        {
            tm->tm_isdst = 0;
            if (tm->tm_year < 0 || tm->tm_year > 1100) return -1;
            if (tm->tm_mon < 0 || tm->tm_mon > 11) return -1;
            if (tm->tm_mday < 1 || tm->tm_mday > 31) return -1;
            if (tm->tm_hour < 0 || tm->tm_hour > 23) return -1;
            if (tm->tm_min < 0 || tm->tm_min > 60) return -1;
            if (tm->tm_sec < 0 || tm->tm_sec > 60) return -1;
#ifdef _MSC_VER
            return _mkgmtime(tm);
#else
            return timegm(tm);
#endif
        }
        int get_tz_offset(const std::string &str)
        {
            if (str == "GMT") return 0;
            throw std::runtime_error("Failed to parse HTTP date. Unknown timezone '" + str + "'");
        }
        int get_month(const std::string &str)
        {
            if (str == "Jan") return 0;
            if (str == "Feb") return 1;
            if (str == "Mar") return 2;
            if (str == "Apr") return 3;
            if (str == "May") return 4;
            if (str == "Jun") return 5;
            if (str == "Jul") return 6;
            if (str == "Aug") return 7;
            if (str == "Sep") return 8;
            if (str == "Oct") return 9;
            if (str == "Nov") return 10;
            if (str == "Dec") return 11;
            throw std::runtime_error("Failed to parse HTTP date. Invalid month '" + str + "'");
        }
        time_t parse_rfc1123(const std::string &str)
        {
            //Sun, 06 Nov 1994 08:49:37 GMT
            std::string ignore, month, tz;
            char sep;
            tm tm;
            time_t t;
            std::stringstream ss(str);

            ss >> ignore >> tm.tm_mday >> month >> tm.tm_year;
            ss >> tm.tm_hour >> sep >> tm.tm_min >> sep >> tm.tm_sec;
            ss >> tz;

            if (ss.fail()) return -1;

            tm.tm_year -= 1900;
            tm.tm_mon = get_month(month);
            t = mkgmtime(&tm);
            if (t >= 0) t -= get_tz_offset(tz);
            return t;
        }
        time_t parse_rfc850(const std::string &str)
        {
            //Sunday, 06-Nov-94 08:49:37 GMT
            std::string ignore, month, tz;
            char sep;
            tm tm;
            time_t t;
            int year2;
            std::stringstream ss(str);
            month.resize(3);

            ss >> ignore;
            ss >> tm.tm_mday >> sep;
            ss.read(&month[0], 3);
            ss >> sep >> year2;
            ss >> tm.tm_hour >> sep >> tm.tm_min >> sep >> tm.tm_sec;
            ss >> tz;

            if (ss.fail()) return -1;

            tm.tm_year = (2000 + year2) - 1900;
            tm.tm_mon = get_month(month);
            t = mkgmtime(&tm);
            if (t >= 0) t -= get_tz_offset(tz);
            return t;
        }
        time_t parse_asctime(const std::string &str)
        {
            //Sun Nov  6 08:49:37 1994
            std::string ignore, month;
            char sep;
            tm tm;
            time_t t;
            std::stringstream ss(str);

            ss >> ignore >> month >> tm.tm_mday;
            ss >> tm.tm_hour >> sep >> tm.tm_min >> sep >> tm.tm_sec;
            ss >> tm.tm_year;

            if (ss.fail()) return -1;

            tm.tm_year -= 1900;
            tm.tm_mon = get_month(month);
            t = mkgmtime(&tm);
            return t;
        }
    }
    time_t parse_time(const std::string &time)
    {
        auto t = parse_rfc1123(time);
        if (t < 0) t = parse_rfc850(time);
        if (t < 0) t = parse_asctime(time);
        if (t < 0) throw std::runtime_error("Failed to parse HTTP time '" + time + "'");
        return t;
    }

}
