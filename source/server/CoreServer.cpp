#include "server/CoreServer.hpp"
#include "core/Parser.hpp"
#include "core/ParserUtils.hpp"
#include "core/Writer.hpp"
#include "net/Net.hpp"
#include "net/Cert.hpp"
#include "net/TcpListenSocket.hpp"
#include "net/TcpSocket.hpp"
#include "net/TlsSocket.hpp"
#include "util/Thread.hpp"
#include "String.hpp"
#include "Error.hpp"
#include <cassert>
#include <cstring>
#include <iostream>

namespace http
{
    class CoreServer::Connection
    {
    public:
        /**Run and delete this connection on completion.
         * This is seperate from the constructor because calling "delete" on an object before its
         * constructor completes is undefined.
         */
        void run(CoreServer *_server, Listener *listener, TcpSocket &&raw_socket)
        {
            try
            {
                server = _server;
                keep_alive = false;
                buffer_len = 0;
                if (listener->tls)
                {
                    //TODO: Make this async/non-blocking
                    socket = std::make_unique<TlsServerSocket>(std::move(raw_socket), listener->tls_cert);
                }
                else
                {
                    socket = std::make_unique<TcpSocket>(std::move(raw_socket));
                }
                start_request();
            }
            catch (const std::exception &)
            {
                delete this;
                return;
            }
        }

        explicit operator bool()const { return (bool)socket; }


    private:
        CoreServer *server;
        std::unique_ptr<Socket> socket;
        bool keep_alive;
        char buffer[RequestParser::LINE_SIZE];
        size_t buffer_len;
        RequestParser parser;

        /**Start receiving a new request.*/
        void start_request()
        {
            parser.reset();
            keep_alive = true;
            start_recv_request();
        }
        /**Start receving part of a request into buffer.
         * Completion calls recv_request.
         */
        void start_recv_request()
        {
            socket->async_recv(server->aio, buffer + buffer_len, sizeof(buffer) - buffer_len,
                std::bind(&CoreServer::Connection::recv_request, this, std::placeholders::_1),
                std::bind(&CoreServer::Connection::io_error, this));
        }
        /**Receive part of a request into buffer.*/
        void recv_request(size_t len)
        {
            if (len == 0)
            {
                // Client closed the connection
                delete this;
            }
            else
            {
                buffer_len += len;

                auto end = parser.read(buffer, buffer + buffer_len);
                buffer_len -= end - buffer;
                memmove(buffer, end, buffer_len);

                if (parser.is_completed()) handle_request();
                else start_recv_request();
            }
        }
        /**Handle the request, called once recv_request has parsed the entire request message.
         * Passes the parsed request to owning server, sends the response and either destroys the
         * connection or starts the next request.
         */
        void handle_request()
        {
            Request req =
            {
                method_from_string(parser.method()),
                parser.uri(),
                Url::parse_request(parser.uri()),
                std::move(parser.headers()),
                std::move(parser.body())
            };

            keep_alive = ieq(req.headers.get("Connection"), "keep-alive");

            Response resp;
            try
            {
                resp = server->handle_request(req);
            }
            catch (const ErrorResponse &err)
            {
                keep_alive = false;
                resp.status.code = (StatusCode)err.status_code();
                resp.body = err.what();
                resp.headers.add("Content-Type", "text/plain");
            }
            catch (const std::exception &err)
            {
                keep_alive = false;
                resp.status.code = SC_INTERNAL_SERVER_ERROR;
                resp.body = err.what();
                resp.headers.add("Content-Type", "text/plain");
            }

            if (resp.status.msg.empty())
            {
                resp.status.msg = default_status_msg(resp.status.code);
            }
            resp.headers.set("Connection", keep_alive ? "keep-alive" : "close");

            // Send response
            auto sc = resp.status.code;
            // For certain response codes, there must not be a message body
            bool message_body_allowed = sc != 204 && sc != 205 && sc != 304;
            //For HEAD requests, Content-Length etc. should be determined, but the body must not be sent
            bool send_message_body = !resp.body.empty() && message_body_allowed && parser.method() != "HEAD";

            if (message_body_allowed)
            {
                //TODO: Support chunked streams in the future
                resp.headers.set("Content-Length", std::to_string(resp.body.size()));
            }
            else if (!resp.body.empty())
            {
                throw std::runtime_error("HTTP forbids this response from having a body");
            }

            add_default_headers(resp);

            // Send response header
            std::stringstream ss;
            write_response_header(ss, resp);
            auto ss_str = ss.str();
            socket->send_all(ss_str.data(), ss_str.size());
            // Send response body
            if (send_message_body)
            {
                socket->send_all(resp.body.data(), resp.body.size());
            }

            if (keep_alive) start_request();
            else
            {
                auto handler = [this]() { delete this; };
                socket->async_disconnect(server->aio, handler, handler);
            }
        }
        /**Called if any recv or send fails. Destroys this connection.*/
        void io_error()
        {
            delete this;
        }
    };

    CoreServer::~CoreServer()
    {
        exit();
    }

    void CoreServer::add_tcp_listener(const std::string &bind, uint16_t port)
    {
        Listener listener = {
            {bind, port},
            false, {}
        };
        listener.socket.set_non_blocking();
        listeners.push_back(std::move(listener));
    }
    void CoreServer::add_tls_listener(const std::string &bind, uint16_t port, const PrivateCert &cert)
    {
        Listener listener = {
            {bind, port},
            true, cert
        };
        listener.socket.set_non_blocking();
        listeners.push_back(std::move(listener));
    }


    void CoreServer::run()
    {
        std::unique_lock<std::mutex> lock(running_mutex, std::try_to_lock);
        if (!lock) throw std::runtime_error("CoreServer::run failed to lock mutex. Is CoreServer already running?");
        for (auto &i : listeners) accept_next(i);

        aio.run();
    }
    void CoreServer::exit()
    {
        std::unique_lock<std::mutex> lock(running_mutex, std::try_to_lock);
        if (!lock)
        {
            aio.exit();
            // Clean up is done by run(). Wait for it.
            lock.lock();
        }
    }
    void CoreServer::accept_next(Listener &listener)
    {
        aio.accept(listener.socket.get(),
            std::bind(&CoreServer::accept, this, std::ref(listener), std::placeholders::_1),
            std::bind(&CoreServer::accept_error, this));
    }
    void CoreServer::accept(Listener &listener, TcpSocket &&sock)
    {
        assert(sock);
        (new Connection())->run(this, &listener, std::move(sock));
        accept_next(listener);
    }
    void CoreServer::accept_error()
    {
        // Rethrow anything except AsyncAborted. Will kill the server instance.
        try { throw; }
        catch (const AsyncAborted &) {}
    }
}
