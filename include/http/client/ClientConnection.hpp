#pragma once
#include "../core/Parser.hpp"
#include "../Request.hpp"
#include "../Response.hpp"
#include <memory>

namespace http
{
    class Socket;
    /**Client side HTTP or HTTPS connection.*/
    class ClientConnection
    {
    public:
        ClientConnection() : socket(nullptr) {}
        /**Construct by taking ownership of an existing socket.*/
        explicit ClientConnection(std::unique_ptr<Socket> &&socket) : socket(nullptr)
        {
            reset(std::move(socket));
        }
        ~ClientConnection();

        /**Use a new socket.*/
        void reset(std::unique_ptr<http::Socket> &&new_socket);
        /**True if expected to currently be connected to the remote server.*/
        bool is_connected()const;
        /**Make a HTTP request using send_request and recv_response.*/
        Response make_request(Request &request)
        {
            send_request(request);
            return recv_response();
        }

        /**Sends a HTTP request.
         * Request will be modified with relevant Content-Length header if needed.
         */
        void send_request(Request &request);
        /**Receieves a HTTP response.*/
        Response recv_response();
    private:
        std::unique_ptr<Socket> socket;
        ResponseParser parser;
    };
}
