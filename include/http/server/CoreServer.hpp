#pragma once
#include "../net/AsyncIo.hpp"
#include "../net/TcpListenSocket.hpp"
#include "../net/Cert.hpp"
#include <atomic>
#include <future>
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
        void add_tls_listener(const std::string &bind, uint16_t port, const PrivateCert &cert);
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
            PrivateCert tls_cert;
        };
        class Connection;

        AsyncIo aio;
        std::vector<Listener> listeners;
        /**Held by run(), preventing exit() from continueing until run() is finished.*/
        std::mutex running_mutex;
        std::mutex handle_mutex;
        std::vector<std::future<void>> in_progress_handlers;

        void accept_next(Listener &listener);
        void accept(Listener &listener, TcpSocket &&sock);
        void accept_error();
    };
}
