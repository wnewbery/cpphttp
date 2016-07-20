#pragma once
#include "../core/Parser.hpp"
#include "../core/Writer.hpp"
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
        /**Make a HTTP request.
         * Request will be modified with relevant Content-Length header if needed.
         */
        Response make_request(Request &request);
    private:
        std::unique_ptr<Socket> socket;
        ResponseParser parser;
    };
}
