#pragma once
#include "Os.hpp"
#include <atomic>
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
        /**Held by the main run thread to block exit() until run() is done. Much like a thread join
         * but allowing the run() thread to return and live on.
         */
        std::mutex exit_mutex;
        #if defined(HTTP_USE_SELECT)
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
        #elif defined(HTTP_USE_IOCP)
        struct CompletionPort
        {
            HANDLE port;
            CompletionPort();
            ~CompletionPort();
        };
        struct Operation
        {
            enum Type
            {
                ACCEPT,RECV,SEND,SEND_ALL
            };
            struct Deleter
            {
                void operator()(Operation *op) { op->delete_this(); }
            };

            WSAOVERLAPPED  overlapped = { 0 };
            WSABUF buffer;
            SOCKET sock;
            Type type;
            ErrorHandler error;

            typedef std::unique_ptr<Operation, Deleter> Ptr;
            std::list<Ptr>::iterator it;

            Operation(SOCKET sock, Type type, const void *buffer, size_t len, ErrorHandler error)
                : overlapped{ 0 }
                , buffer{ (unsigned)len, (char*)buffer }
                , sock(sock)
                , type(type), error(error)
            {}
            void delete_this();
        };
        struct Accept : public Operation
        {
            AcceptHandler handler;
            SOCKET client_sock;
            static const DWORD addr_len = (DWORD)sizeof(sockaddr_storage) + 16;
            char accept_buffer[addr_len + addr_len];
            char remote_addr_buf[sizeof(sockaddr_storage) + 16];
            Accept(SOCKET sock, AcceptHandler handler, ErrorHandler error);
            ~Accept();
        };
        struct Recv : public Operation
        {
            SendHandler handler;

            Recv(SOCKET sock, void *buffer, size_t len, SendHandler handler, ErrorHandler error)
                : Operation(sock, RECV, buffer, len, error), handler(handler)
            {}
        };
        struct Send : public Operation
        {
            SendHandler handler;

            Send(SOCKET sock, const void *buffer, size_t len, SendHandler handler, ErrorHandler error)
                : Operation(sock, SEND, buffer, len, error), handler(handler)
            {}
        };
        struct SendAll : public Operation
        {
            SendHandler handler;
            size_t len;
            size_t sent;

            SendAll(SOCKET sock, const void *buffer, size_t len, SendHandler handler, ErrorHandler error)
                : Operation(sock, SEND_ALL, buffer, len, error)
                , handler(handler)
                , len(len), sent(0)
            {}
        };
        CompletionPort iocp;
        std::mutex mutex;
        std::atomic<bool> running;
        std::list<Operation::Ptr> inprogess_operations;
        void iocp_loop();
        /**Prepare to start some operation for a socket.
         * Makes sure the socket is associated, checks running and updates inprogess_operations.
         * If AsyncIo is exiting, invokes error with AsyncAborted, then returns false.
         */
        bool start_operation(SOCKET socket, ErrorHandler error);
        void send_all_next(std::unique_ptr<SendAll, Operation::Deleter> send, size_t sent);
        #else
            #error No AsyncIo method defined
        #endif

        /**Calls the error handler with an active AsyncAborted exception.*/
        void do_abort(const ErrorHandler &error)
        {
            try { throw AsyncAborted(); }
            catch (const AsyncAborted &e) { call_error(e, error); }
        }
        /**Calls the error handler with the current active exception.*/
        void call_error(const std::exception &e, const ErrorHandler &handler);

    };
}
