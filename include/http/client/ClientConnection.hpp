#pragma once
#include "../core/Parser.hpp"
#include "../Request.hpp"
#include "../Response.hpp"
#include <memory>

namespace http
{
    class Socket;
    class ClientConnection
    {
    public:
        ClientConnection() : socket(nullptr) {}
        ClientConnection(std::unique_ptr<Socket> &&socket) : socket(nullptr)
        {
            reset(std::move(socket));
        }
        ~ClientConnection();

        void reset(std::unique_ptr<http::Socket> &&new_socket);
        bool is_connected()const { return socket != nullptr; }
        /**Make a HTTP request.
         * Request will be modified with relevant Content-Length header if needed.
         */
        Response make_request(Request &request)
        {
            send_request(request);
            return recv_response();
        }

        void send_request(Request &request);
        Response recv_response();
    private:
        std::unique_ptr<Socket> socket;
        ResponseParser parser;
    };
}
