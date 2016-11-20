#pragma once
#include "../net/TcpListenSocket.hpp"
#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
namespace http
{
    class Socket;
    class TcpSocket;
    class Request;
    class Response;
    class ParserError;
    //TODO: add clean-shutdown functionality
    /**A minimalistic server implementation.
     * Uses multiple threads for connections, but contains no logic for
     * processing the recieved requests.
     */
    class CoreServer
    {
    public:
        /**Create a server with no initial listeners.*/
        CoreServer(){}
        /**Listen on a specific interface local IP and port.*/
        CoreServer(const std::string &bind, uint16_t port)
            : CoreServer()
        {
            add_tcp_listener(bind, port);
        }
        /**Listen on all interfaces for a given port.*/
        explicit CoreServer(uint16_t port) : CoreServer("0.0.0.0", port) {}
        virtual ~CoreServer();

        /**Add a listener before calling run.*/
        void add_tcp_listener(const std::string &bind, uint16_t port);
        /**Add a TLS listener before calling run.*/
        void add_tls_listener(const std::string &bind, uint16_t port, const std::string &hostname);
        void run();
        /**Signals the thread in run() and all workers to exit, then waits for them.*/
        void exit();

    protected:
        /**Process the request. This may be called by multiple internal threads.
         * Any uncaught exception will kill the thread.
         */
        virtual Response handle_request(Request &request)=0;
        /**Create an error response page.
         * This may be called by CoreServer instead of handle_request if there was an issue reading
         * the HTTP request.
         *
         * Any uncaught exception will kill the thread.
         */
        virtual Response parser_error_page(const ParserError &err)=0;
    private:
        struct Listener
        {
            TcpListenSocket socket;
            bool tls;
            std::string tls_hostname;
        };
        // TODO: Refactor. On Windows can use events, on Linux can use pipes
        class SignalSocket
        {
        public:
            SignalSocket();
            ~SignalSocket();
            void create();
            void destroy();
            void signal();
            SOCKET get();
        private:
            SOCKET send;
            SOCKET recv;
        };
        class Thread
        {
        public:
            Thread(CoreServer *server, Listener *listener, TcpSocket &&socket);
            ~Thread();

            void main_tcp(TcpSocket &&socket);
            void main_tls(Listener *listener, TcpSocket &&socket);
            void run();

            std::thread thread;
            std::atomic<bool> running;
            CoreServer *server;
            std::unique_ptr<Socket> socket;
#ifndef _WIN32
            SignalSocket exit_socket;
#endif
        };

        std::vector<Listener> listeners;
        std::list<Thread> threads;
        SignalSocket exit_socket;
        /**Held by run(), preventing exit() from continueing until run() is finished.*/
        std::mutex running_mutex;

        void accept(Listener &listener);
    };
}
