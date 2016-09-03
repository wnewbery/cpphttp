#pragma once
#include "../net/TcpListenSocket.hpp"
#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
namespace http
{
    class Socket;
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
        CoreServer(const std::string &bind, uint16_t port);
        CoreServer(uint16_t port) : CoreServer("0.0.0.0", port) {}
        ~CoreServer();

        void run();

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
        class Thread
        {
        public:
            Thread(CoreServer *server, std::unique_ptr<Socket> &&socket);

            void main();
            void main2();

            std::thread thread;
            std::atomic<bool> running;
            CoreServer *server;
            std::unique_ptr<Socket> socket;
        };

        TcpListenSocket listen_socket;
        std::list<Thread> threads;
    };
}
