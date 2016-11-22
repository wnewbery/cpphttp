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
#include "Error.hpp"
#include <cassert>
#include <cstring>
#include <iostream>

namespace http
{
    namespace
    {
        template<class Arr>
        int set_fdset(fd_set &set, Arr &sockets)
        {
            SOCKET max = 0;
            FD_ZERO(&set);
            for (auto sock : sockets)
            {
                if (sock > max) max = sock;
                FD_SET(sock, &set);
            }
            return (int)max;
        }
        template<class... Sockets>
        int set_fdset_v(fd_set &set, Sockets...sockets)
        {
            SOCKET max = 0;
            FD_ZERO(&set);
            for (auto sock : {sockets...})
            {
                if (sock > max) max = sock;
                FD_SET(sock, &set);
            }
            return (int)max;
        }
    }
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
        exit_socket.create();

        fd_set read_set;
        while (true)
        {
            FD_ZERO(&read_set);
            SOCKET max = exit_socket.get();
            FD_SET(exit_socket.get(), &read_set);
            for (auto &listener : listeners)
            {
                auto fd = listener.socket.get();
                if (fd > max) max = fd;
                FD_SET(fd, &read_set);
            }

            // (int)max is safe. Even if MS changes the implementation to return higher value sockets,
            // Windows select ignores this parameter. On Linux file descriptors are of type int.
            auto ret = select((int)max + 1, &read_set, nullptr, nullptr, nullptr);
            if (ret < 0) throw SocketError(last_net_error());
            if (FD_ISSET(exit_socket.get(), &read_set)) break;
            for (auto &listener : listeners)
            {
                if (FD_ISSET(listener.socket.get(), &read_set))
                {
                    accept(listener);
                }
            }
        }
        // Exiting, wait for workers and clean up
#ifndef _WIN32
        for (auto &thread : threads)
            thread.exit_socket.signal();
#endif
        for (auto &thread : threads)
            if (thread.thread.joinable())
                thread.thread.join();
        threads.clear();
        exit_socket.destroy();
    }
    void CoreServer::exit()
    {
        std::unique_lock<std::mutex> lock(running_mutex, std::try_to_lock);
        if (!lock)
        {
            exit_socket.signal();
            // Clean up is done by run(). Wait for it.
            lock.lock();
        }
    }
    void CoreServer::accept(Listener &listener)
    {
        auto sock = listener.socket.accept();
        if (sock) // If the client did not already disconnect
        {
            // New thread
            threads.emplace_back(this, &listener, std::move(sock));
        }

        // Clear out any dead entries
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


    CoreServer::Thread::Thread(CoreServer *server, Listener *listener, TcpSocket &&socket)
        : thread(), running(true), server(server), socket()
    {
#ifndef _WIN32
        exit_socket.create();
#endif
        if (listener->tls)
        {
            thread = std::thread(&CoreServer::Thread::main_tls, this, listener, std::move(socket));
        }
        else
        {
            thread = std::thread(&CoreServer::Thread::main_tcp, this, std::move(socket));
        }
    }
    CoreServer::Thread::~Thread() {}

    void CoreServer::Thread::main_tcp(TcpSocket &&new_socket)
    {
        try
        {
            set_thread_name("http::CoreServer worker");
            socket.reset(new TcpSocket(std::move(new_socket)));
            run();
            running = false;
        }
        catch (const std::exception &e)
        {
            running = false;
            std::cerr << "Worker exception: " << e.what() << std::endl;
        }
    }
    void CoreServer::Thread::main_tls(Listener *listener, TcpSocket &&new_socket)
    {
        try
        {
            set_thread_name("http::CoreServer worker");
            //TODO: Make an asyncronous TLS negotiation
            socket.reset(new TlsServerSocket(std::move(new_socket), listener->tls_cert));
            run();
            running = false;
        }
        catch (const std::exception &e)
        {
            running = false;
            std::cerr << "Worker exception: " << e.what() << std::endl;
        }
    }

    void CoreServer::Thread::run()
    {
        char buffer[RequestParser::LINE_SIZE];
        size_t buffer_len = 0;
        RequestParser parser;
        while (true)
        {
            if (!buffer_len && !socket->recv_pending())
            {
                fd_set read_set;
#ifdef _WIN32
                auto &exit_socket = server->exit_socket;
#endif
                auto max = set_fdset_v(read_set, exit_socket.get(), socket->get());
                auto ret = select(max + 1, &read_set, nullptr, nullptr, nullptr);
                if (ret <= 0) throw std::runtime_error("select failed");
                if (FD_ISSET(exit_socket.get(), &read_set)) break;
                assert(FD_ISSET(socket->get(), &read_set));
            }

            //Start request
            //TODO: Idle timeout
            //TODO: Allow aborting in-progress requests
            Response resp;
            resp.body.clear();
            try
            {
                //parse
                parser.reset();
                while (!parser.is_completed())
                {
                    auto recved = socket->recv(buffer + buffer_len, sizeof(buffer) - buffer_len);
                    if (!recved)
                    {
                        if (parser.state() == BaseParser::START) break;
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
            send_response(socket.get(), parser.method(), resp);
        }
    }

    CoreServer::SignalSocket::SignalSocket()
        : send(INVALID_SOCKET), recv(INVALID_SOCKET)
    {}
    CoreServer::SignalSocket::~SignalSocket()
    {
        destroy();
    }
    void CoreServer::SignalSocket::create()
    {
        assert(send == INVALID_SOCKET && recv == INVALID_SOCKET);

        TcpListenSocket listen("127.0.0.1", 0);
        sockaddr_in addr;
        auto len = (socklen_t)sizeof(addr);
        getsockname(listen.get(), (sockaddr*)&addr, &len);

        send = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (send == INVALID_SOCKET) throw std::runtime_error("SignalSocket socket failed");
        if (connect(send, (sockaddr*)&addr, len))
            throw std::runtime_error("SignalSocket connect failed");

        recv = ::accept(listen.get(), nullptr, nullptr);
        if (recv == INVALID_SOCKET) throw std::runtime_error("SignalSocket accept failed");
    }
    void CoreServer::SignalSocket::destroy()
    {
        if (send != INVALID_SOCKET) closesocket(send);
        if (recv != INVALID_SOCKET) closesocket(recv);
        send = recv = INVALID_SOCKET;
    }
    void CoreServer::SignalSocket::signal()
    {
        assert(send != INVALID_SOCKET && recv != INVALID_SOCKET);
        char val[1] = {'X'};
        if (::send(send, val, 1, 0) != 1)
            throw std::runtime_error("SignalSocket signal failed");
    }
    SOCKET CoreServer::SignalSocket::get()
    {
        return recv;
    }
}
