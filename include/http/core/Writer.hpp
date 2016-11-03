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
    /**Writes the header text to the output stream.*/
    inline void write_headers(std::ostream &os, const Headers &headers)
    {
        for (auto &header : headers)
        {
            os << header.first << ": " << header.second << "\r\n";
        }
        os << "\r\n";
    }
    /**Writes the HTTP request first line and headers to the output stream.*/
    inline void write_request_header(std::ostream &os, const Request &request)
    {
        os << to_string(request.method) << " ";
        if (!request.raw_url.empty()) os << request.raw_url;
        else request.url.encode_request(os);
        os << " HTTP/1.1\r\n";
        write_headers(os, request.headers);
    }
    /**Writes the HTTP response first line and headers to the output stream.*/
    inline void write_response_header(std::ostream &os, const Response &response)
    {
        os << "HTTP/1.1 " << response.status.code << " " << response.status.msg << "\r\n";
        write_headers(os, response.headers);
    }
    /**Adds basic default headers to a request or response to be sent.
     * Currently this is just the "Date" header.
     */
    template<class T> void add_default_headers(T &message)
    {
        Headers &headers = message.headers;
        headers.set("Date", format_time(time(nullptr)));
    }

    /**Sends a HTTP client side request to the socket using Socket::send_all.*/
    inline void send_request(Socket *socket, Request &request)
    {
        if (!request.body.empty())
            request.headers.set("Content-Length", std::to_string(request.body.size()));
        std::stringstream ss;
        write_request_header(ss, request);
        auto ss_str = ss.str();
        socket->send_all(ss_str.data(), ss_str.size());
        if (!request.body.empty()) socket->send_all(request.body.data(), request.body.size());
    }

    /**Sends a HTTP server side response to the socket using Socket::send_all.*/
    inline void send_response(Socket *socket, const std::string &req_method, Response &response)
    {
        auto sc = response.status.code;
        //For certain response codes, there must not be a message body
        bool message_body_allowed = sc != 204 && sc != 205 && sc != 304;

       //For HEAD requests, Content-Length etc. should be determined, but the body must not be sent
        bool send_message_body = message_body_allowed && req_method != "HEAD";

        if (message_body_allowed)
        {
            //TODO: Support chunked streams in the future
            response.headers.set("Content-Length", std::to_string(response.body.size()));
        }
        else if (!response.body.empty())
        {
            throw std::runtime_error("HTTP forbids this response from having a body");
        }

        add_default_headers(response);
        std::stringstream ss;
        write_response_header(ss, response);
        auto ss_str = ss.str();
        socket->send_all(ss_str.data(), ss_str.size());

        if (send_message_body && !response.body.empty())
        {
            socket->send_all(response.body.data(), response.body.size());
        }
    }
}
