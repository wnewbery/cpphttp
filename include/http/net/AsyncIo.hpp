#pragma once
#include "Os.hpp"
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
namespace http
{
    class AsyncAborted : public std::runtime_error
    {
    public:
        AsyncAborted() : std::runtime_error("Aborted") {}
    };
    class TcpSocket;
    class AsyncIo
    {
    public:
        typedef std::function<void()> ErrorHandler;
        typedef std::function<void(TcpSocket &&sock)> AcceptHandler;
        typedef std::function<void(size_t len)> RecvHandler;
        typedef std::function<void(size_t len)> SendHandler;

        AsyncIo();
        ~AsyncIo();

        AsyncIo(const AsyncIo&) = delete;
        AsyncIo& operator = (const AsyncIo&) = delete;

        void run();
        void exit();

        void accept(SOCKET sock, AcceptHandler handler, ErrorHandler error);
        void recv(SOCKET sock, void *buffer, size_t len, RecvHandler handler, ErrorHandler error);
        void send(SOCKET sock, const void *buffer, size_t len, SendHandler handler, ErrorHandler error);
        void send_all(SOCKET sock, const void *buffer, size_t len, SendHandler handler, ErrorHandler error);
    private:
        // TODO: Refactor. On Windows can use events, on Linux can use pipes
        class SignalSocket
        {
        public:
            SignalSocket();
            ~SignalSocket();
            void create();
            void destroy();
            void signal();
            void clear();
            SOCKET get();
        private:
            SOCKET send;
            SOCKET recv;
        };

        struct Accept
        {
            SOCKET sock;
            AcceptHandler handler;
            ErrorHandler error;
        };
        struct Recv
        {
            SOCKET sock;
            void *buffer;
            size_t len;
            RecvHandler handler;
            ErrorHandler error;
        };
        struct Send
        {
            bool all;
            SOCKET sock;
            const void *buffer;
            size_t len;
            size_t sent;
            SendHandler handler;
            ErrorHandler error;
        };

        SignalSocket signal;
        bool exiting;
        std::mutex exit_mutex;
        std::mutex mutex;
        /**In-progress operations.
         * If a single socket has multiple read or write operations, they are processed in order.
         */
        struct
        {
            std::list<Accept> accept;
            std::unordered_map<SOCKET, std::list<Recv>> recv;
            std::unordered_map<SOCKET, std::list<Send>> send;
        }in_progress;
        /**New operations added by other threads and no yet included in the select() loop.*/
        struct
        {
            std::vector<Accept> accept;
            std::vector<Recv> recv;
            std::vector<Send> send;
        }new_operations;

        /**Calls the error handler with an active AsyncAborted exception.*/
        template<class T> void do_abort(T &op)
        {
            try { throw AsyncAborted(); }
            catch (const AsyncAborted &) { op.error(); }
        }
    };
}
