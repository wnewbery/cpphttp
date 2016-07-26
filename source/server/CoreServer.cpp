#include "server/CoreServer.hpp"
#include "core/Parser.hpp"
#include "core/ParserUtils.hpp"
#include "core/Writer.hpp"
#include "net/TcpListenSocket.hpp"
#include "net/TcpSocket.hpp"

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
            threads.emplace_back(this, std::move(sock));
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
        catch (const std::exception &e)
        {
            running = false;
        }
    }

    void CoreServer::Thread::main2()
    {
        char buffer[RequestParser::LINE_SIZE];
        RequestParser parser;
        while (true)
        {
            //parse
            parser.reset();
            size_t buffer_len = 0;
            while (!parser.is_completed())
            {
            }
        }
    }
}
