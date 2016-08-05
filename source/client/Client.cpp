#include "client/Client.hpp"
#include "net/Net.hpp"
#include "net/Socket.hpp"
#include <cassert>
namespace http
{
    Client::Client(const std::string &host, uint16_t port, bool tls)
        : _def_headers(), _factory(nullptr), conn()
        , host(host), port(port), tls(tls)
    {
        _def_headers.set_default("Host", host + ":" + std::to_string(port));
    }

    Client::~Client()
    {
    }

    Response Client::make_request(Request & request)
    {
        assert(_factory);

        for (auto &header : _def_headers)
        {
            request.headers.set_default(header.first, header.second);
        }

        // Send
        bool sent = false;
        if (conn.is_connected())
        {
            try
            {
                conn.send_request(request);
                sent = true;
            }
            catch (const SocketError &) {}
        }
        if (!sent)
        {
            conn.reset(nullptr);
            conn.reset(_factory->connect(host, port, tls));
            conn.send_request(request);
        }

        //Response
        return conn.recv_response();
    }
}
