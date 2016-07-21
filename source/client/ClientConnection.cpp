#include "client/ClientConnection.hpp"
#include "core/Writer.hpp"
#include "net/Socket.hpp"
#include <cassert>
#include <cstring>
namespace http
{
    ClientConnection::~ClientConnection()
    {}

    void ClientConnection::reset(std::unique_ptr<http::Socket> &&new_socket)
    {
        socket = std::move(new_socket);
    }

    void ClientConnection::send_request(Request &request)
    {
        assert(socket);
        add_default_headers(request);
        http::send_request(socket.get(), request);

        parser.reset(request.method);
    }

    Response ClientConnection::recv_response()
    {
        assert(socket);
        char buffer[ResponseParser::LINE_SIZE];
        size_t buffer_capacity = sizeof(buffer);
        size_t buffer_len = 0;

        while (!parser.is_completed())
        {
            auto recv = socket->recv(buffer + buffer_len, buffer_capacity - buffer_len);
            if (recv == 0) throw std::runtime_error("Server disconnected before response was complete");
            buffer_len += recv;

            auto end = buffer + buffer_len;
            auto read_end = parser.read(buffer, end);
            if (read_end == buffer && buffer_len == buffer_capacity)
            {
                throw std::runtime_error("Response line too large");
            }

            buffer_len = (size_t)(end - read_end);
            memmove(buffer, read_end, buffer_len);
        }
        if (buffer_len != 0) throw std::runtime_error("Unexpected content after response");

        Response resp;
        resp.headers = parser.headers();
        resp.body = parser.body();
        resp.status = parser.status();
        return resp;
    }
}
