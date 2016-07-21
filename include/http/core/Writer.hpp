#pragma once
#include "Request.hpp"
#include "Response.hpp"
#include "Headers.hpp"
#include "Method.hpp"
#include "Time.hpp"
#include "net/Socket.hpp"
#include <ostream>
#include <sstream>

namespace http
{
    inline void write_headers(std::ostream &os, const Headers &headers)
    {
        for (auto &header : headers)
        {
            os << header.first << ": " << header.second << "\r\n";
        }
        os << "\r\n";
    }
    inline void write_request_header(std::ostream &os, const Request &request)
    {
        os << to_string(request.method) << " ";
        if (!request.raw_url.empty()) os << request.raw_url;
        else request.url.encode_request(os);
        os << " HTTP/1.1\r\n";
        write_headers(os, request.headers);
    }
    inline void write_response_header(std::ostream &os, const Response &response)
    {
        os << "HTTP/1.1 " << response.status.code << " " << response.status.msg << "\r\n";
        write_headers(os, response.headers);
    }

    template<class T> void add_default_headers(T &message)
    {
        Headers &headers = message.headers;
        headers.set("Date", format_time(time(nullptr)));
        if (!message.body.empty()) headers.set("Content-Length", std::to_string(message.body.size()));
    }

    inline void send_request(Socket *socket, Request &request)
    {
        add_default_headers(request);
        std::stringstream ss;
        write_request_header(ss, request);
        auto ss_str = ss.str();
        socket->send_all(ss_str.data(), ss_str.size());
        if (!request.body.empty()) socket->send_all(request.body.data(), request.body.size());
    }

    inline void send_response(Socket *socket, Response &response)
    {
        add_default_headers(response);
        std::stringstream ss;
        write_response_header(ss, response);
        auto ss_str = ss.str();
        socket->send_all(ss_str.data(), ss_str.size());
        if (!response.body.empty()) socket->send_all(response.body.data(), response.body.size());
    }
}
