#include "server/CoreServer.hpp"
#include "core/Parser.hpp"
#include "core/ParserUtils.hpp"
#include "core/Writer.hpp"
#include "net/TcpListenSocket.hpp"
#include "net/TcpSocket.hpp"
#include "Error.hpp"
#include <cstring>

namespace http
{
    CoreServer::CoreServer(const std::string & bind, uint16_t port)
        : listen_socket(bind, port), threads()
    {}
    
    void CoreServer::run()
    {
        while (true)
        {
            auto sock = listen_socket.accept();
            
            //new thread
            threads.emplace_back(this, std::unique_ptr<Socket>(new TcpSocket(std::move(sock))));
            //clear out any dead entries
            for (auto thread = threads.begin(); thread != threads.end(); )
            {
                if (thread->running) ++thread;
                else
                {
                    thread->thread.join();
                    thread = threads.erase(thread);
                }
            }
        }
    }


    CoreServer::Thread::Thread(CoreServer *server, std::unique_ptr<Socket> &&socket)
        : thread(), running(true), server(server), socket(std::move(socket))
    {
        thread = std::thread(&CoreServer::Thread::main, this);
    }

    void CoreServer::Thread::main()
    {
        try
        {
            main2();
            running = false;
        }
        catch (const std::exception &)
        {
            running = false;
        }
    }

    void CoreServer::Thread::main2()
    {
        char buffer[RequestParser::LINE_SIZE];
        size_t buffer_len = 0;
        RequestParser parser;
        while (true)
        {
            Response resp;
            try
            {
                //parse
                parser.reset();
                while (!parser.is_completed())
                {
                    auto recved = socket->recv(buffer + buffer_len, sizeof(buffer) - buffer_len);
                    if (!recved)
                    {
                        if (parser.state() == BaseParser::START) return;
                        else throw std::runtime_error("Unexpected client disconnect");
                    }
                    buffer_len += recved;

                    auto end = parser.read(buffer, buffer + buffer_len);
                    buffer_len -= end - buffer;
                    memmove(buffer, end, buffer_len);
                }
                //Handle
                Request req =
                {
                    method_from_string(parser.method()),
                    parser.uri(),
                    Url::parse_request(parser.uri()),
                    parser.headers(),
                    parser.body()
                };

                resp = server->handle_request(req);
            }
            catch (const ParserError &err)
            {
                int status = err.status_code();
                if (status <= 0) status = 400;
                resp.status.code = (StatusCode)status;
                resp.body = err.what();
                resp.headers.add("Content-Type", "text/plain");
            }
            catch (const ErrorResponse &err)
            {
                resp.status.code = (StatusCode)err.status_code();
                resp.body = err.what();
                resp.headers.add("Content-Type", "text/plain");
            }
            catch (const std::exception &err)
            {
                resp.status.code = SC_INTERNAL_SERVER_ERROR;
                resp.body = err.what();
                resp.headers.add("Content-Type", "text/plain");
            }
            if (resp.status.msg.empty())
            {
                resp.status.msg = default_status_msg(resp.status.code);
            }

            //Send response
            send_response(socket.get(), resp);
        }
    }
}
