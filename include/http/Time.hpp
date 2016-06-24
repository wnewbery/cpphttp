#pragma once
#include <ctime>
#include <string>

namespace http
{
    /**Format date and time for using in HTTP headers such as Date and Last-Modified.*/
    std::string format_time(time_t utc);

    /**Parses date and time as specified by HTTP headers such as Date and Last-Modified.*/
    time_t parse_time(const std::string &time);
}
