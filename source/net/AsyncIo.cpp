#include "net/AsyncIo.hpp"
#include "net/Net.hpp"
#include "net/TcpSocket.hpp"
#include "net/TcpListenSocket.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <cassert>
#include <mswsock.h> // AcceptEx
namespace http
{
    #ifdef HTTP_USE_SELECT
    namespace
    {
        struct FdSets
        {
            int nfds;
            fd_set read_set;
            fd_set write_set;

            FdSets() : nfds(0), read_set(), write_set()
            {
                FD_ZERO(&read_set);
                FD_ZERO(&write_set);
            }
            void read(SOCKET sock)
            {
                if (sock + 1 > nfds) nfds = (int)sock + 1;
                FD_SET(sock, &read_set);
            }
            void write(SOCKET sock)
            {
                if (sock + 1 > nfds) nfds = (int)sock + 1;
                FD_SET(sock, &write_set);
            }

            bool check_read(SOCKET sock)const
            {
                return FD_ISSET(sock, &read_set) != 0;
            }
            bool check_write(SOCKET sock)const
            {
                return FD_ISSET(sock, &write_set) != 0;
            }
        };
    }
    AsyncIo::SignalSocket::SignalSocket()
        : send(INVALID_SOCKET), recv(INVALID_SOCKET)
    {}
    AsyncIo::SignalSocket::~SignalSocket()
    {
        destroy();
    }
    void AsyncIo::SignalSocket::create()
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
    void AsyncIo::SignalSocket::destroy()
    {
        if (send != INVALID_SOCKET) closesocket(send);
        if (recv != INVALID_SOCKET) closesocket(recv);
        send = recv = INVALID_SOCKET;
    }
    void AsyncIo::SignalSocket::signal()
    {
        assert(send != INVALID_SOCKET && recv != INVALID_SOCKET);
        char val[1] = { 'X' };
        if (::send(send, val, 1, 0) != 1)
            throw std::runtime_error("SignalSocket signal failed");
    }
    void AsyncIo::SignalSocket::clear()
    {
        char buffer[1];
        ::recv(recv, buffer, 1, 0);
    }
    SOCKET AsyncIo::SignalSocket::get()
    {
        return recv;
    }

    AsyncIo::AsyncIo()
        : exiting(false)
    {
        signal.create();
    }
    AsyncIo::~AsyncIo()
    {
    }
    void AsyncIo::run()
    {
        std::unique_lock<std::mutex> exit_lock(exit_mutex);
        while (!exiting)
        {
            // pending
            {
                std::unique_lock<std::mutex> lock(mutex);
                for (auto &&op : new_operations.accept)
                    in_progress.accept.emplace_back(std::move(op));
                for (auto &&op : new_operations.recv)
                    in_progress.recv[op.sock].emplace_back(std::move(op));
                for (auto &&op : new_operations.send)
                    in_progress.send[op.sock].emplace_back(std::move(op));
                new_operations.accept.clear();
                new_operations.recv.clear();
                new_operations.send.clear();
            }
            // fd_set
            FdSets fd_sets;
            fd_sets.read(signal.get());
            for (auto &i : in_progress.accept) fd_sets.read(i.sock);
            for (auto &i : in_progress.recv) fd_sets.read(i.first);
            for (auto &i : in_progress.send) fd_sets.write(i.first);
            // select, wait for a signal for new operation, or for a current operator to complete
            auto select_ret = select(fd_sets.nfds, &fd_sets.read_set, &fd_sets.write_set, nullptr, nullptr);
            if (select_ret < 0) throw std::runtime_error("select failed");
            if (fd_sets.check_read(signal.get()))
                signal.clear();
            // Process accept
            for (auto i = in_progress.accept.begin(); i != in_progress.accept.end();)
            {
                if (fd_sets.check_read(i->sock))
                {
                    try
                    {
                        sockaddr_storage client_addr = { 0 };
                        socklen_t client_addr_len = (socklen_t)sizeof(client_addr);
                        auto client_socket = ::accept(i->sock, (sockaddr*)&client_addr, &client_addr_len);
                        if (client_socket != INVALID_SOCKET)
                        {
                            TcpSocket client(client_socket, (sockaddr*)&client_addr);
                            i->handler(std::move(client));
                            i = in_progress.accept.erase(i);
                            continue;
                        }
                        else
                        {
                            auto err = last_net_error();
                            if (!would_block(err)) throw SocketError("socket accept failed", last_net_error());
                        }
                    }
                    catch (const std::exception &)
                    {
                        i->error();
                    }
                }
                ++i;
            }
            // Process recv
            for (auto i = in_progress.recv.begin(); i != in_progress.recv.end();)
            {
                auto &op = i->second.front();
                if (fd_sets.check_read(op.sock))
                {
                    try
                    {
                        if (op.len > (size_t)std::numeric_limits<int>::max())
                            op.len = (size_t)std::numeric_limits<int>::max();
                        auto ret = ::recv(op.sock, (char*)op.buffer, (int)op.len, 0);
                        if (ret < 0) throw SocketError(last_net_error());
                        op.handler((size_t)ret);
                        i->second.pop_front();
                        if (i->second.empty())
                        {
                            i = in_progress.recv.erase(i);
                            continue;
                        }
                    }
                    catch (const std::exception &)
                    {
                        op.error();
                    }
                }
                ++i;
            }
            // Process send
            for (auto i = in_progress.send.begin(); i != in_progress.send.end();)
            {
                auto &op = i->second.front();
                if (fd_sets.check_write(op.sock))
                {
                    try
                    {
                        int len;
                        if (op.len - op.sent > (size_t)std::numeric_limits<int>::max())
                            len = (size_t)std::numeric_limits<int>::max();
                        else len = (int)(op.len - op.sent);

                        auto ret = ::send(op.sock, (const char*)op.buffer + op.sent, len, 0);
                        if (ret <= 0) throw SocketError(last_net_error());
                        op.sent += ret;
                        if (!op.all || op.sent >= op.len)
                        {
                            op.handler(op.sent);
                            i->second.pop_front();
                            if (i->second.empty())
                            {
                                i = in_progress.send.erase(i);
                                continue;
                            }
                        }
                    }
                    catch (const std::exception &e)
                    {
                        call_error(e, send->error);
                    }
                }
                ++i;
            }
        }

        // Abort
        {
            std::unique_lock<std::mutex> lock(mutex);
            for (auto &i : in_progress.accept) do_abort(i.error);
            for (auto &i : in_progress.recv) for (auto &j : i.second) do_abort(j.error);
            for (auto &i : in_progress.send) for (auto &j : i.second) do_abort(j.error);
            for (auto &i : new_operations.accept) do_abort(i.error);
            for (auto &i : new_operations.recv) do_abort(i.error);
            for (auto &i : new_operations.send) do_abort(i.error);

            in_progress.accept.clear();
            in_progress.recv.clear();
            in_progress.send.clear();

            new_operations.accept.clear();
            new_operations.recv.clear();
            new_operations.send.clear();
        }
    }
    void AsyncIo::exit()
    {
        exiting = true;
        signal.signal();
        std::unique_lock<std::mutex> lock(exit_mutex);
    }
    void AsyncIo::accept(SOCKET sock, AcceptHandler handler, ErrorHandler error)
    {
        std::unique_lock<std::mutex> lock(mutex);
        new_operations.accept.emplace_back(Accept{ sock, handler, error });
    }
    void AsyncIo::recv(SOCKET sock, void *buffer, size_t len, RecvHandler handler, ErrorHandler error)
    {
        std::unique_lock<std::mutex> lock(mutex);
        new_operations.recv.emplace_back(Recv{sock, buffer, len, handler, error});
    }
    void AsyncIo::send(SOCKET sock, const void *buffer, size_t len, SendHandler handler, ErrorHandler error)
    {
        std::unique_lock<std::mutex> lock(mutex);
        new_operations.send.emplace_back(Send{false, sock, buffer, len, 0, handler, error});
    }
    void AsyncIo::send_all(SOCKET sock, const void *buffer, size_t len, SendHandler handler, ErrorHandler error)
    {
        std::unique_lock<std::mutex> lock(mutex);
        new_operations.send.emplace_back(Send{ true, sock, buffer, len, 0, handler, error });
    }
    #endif


    #ifdef HTTP_USE_IOCP
    AsyncIo::CompletionPort::CompletionPort()
    {
        port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
        if (!port) throw WinError("CreateIoCompletionPort");
    }
    AsyncIo::CompletionPort::~CompletionPort()
    {
        CloseHandle(port);
    }
    void AsyncIo::Operation::delete_this()
    {
        switch (type)
        {
        case ACCEPT: return delete (Accept*)this;
        case RECV: return delete (Recv*)this;
        case SEND: return delete (Send*)this;
        default:
            assert(type == SEND_ALL);
            return delete (SendAll*)this;
        }
    }
    AsyncIo::Accept::Accept(SOCKET sock, AcceptHandler handler, ErrorHandler error)
        : Operation(sock, ACCEPT, nullptr, 0, error)
        , handler(handler)
        , client_sock(create_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
    {
        if (client_sock == INVALID_SOCKET) throw SocketError("socket");
    }
    AsyncIo::Accept::~Accept()
    {
        if (client_sock != INVALID_SOCKET) closesocket(client_sock);
    }
    AsyncIo::AsyncIo()
        : iocp(), exit_mutex(), running(true), inprogess_operations()
    {
    }
    AsyncIo::~AsyncIo()
    {
        assert(!running);
        assert(inprogess_operations.empty());
    }
    void AsyncIo::run()
    {
        std::unique_lock<std::mutex> lock(exit_mutex);
        iocp_loop();
    }
    void AsyncIo::exit()
    {
        running = false;
        {
            // Cancel anything in progress
            std::unique_lock<std::mutex> lock(mutex);
            for (auto &op : inprogess_operations)
                CancelIoEx((HANDLE)op->sock, NULL);
        }
        PostQueuedCompletionStatus(iocp.port, 0, NULL, NULL);
        std::unique_lock<std::mutex> lock(exit_mutex);
        assert(inprogess_operations.empty());
    }

    void AsyncIo::accept(SOCKET sock, AcceptHandler handler, ErrorHandler error)
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (!start_operation(sock, error)) return;
        std::unique_ptr<Accept, Operation::Deleter> op(new Accept(sock, handler, error));
        auto ret = AcceptEx(sock, op->client_sock, op->accept_buffer, 0, op->addr_len, op->addr_len, nullptr, &op->overlapped);
        auto err = WSAGetLastError();
        if (!ret || err == WSA_IO_PENDING)
        {
            auto p = op.get();
            inprogess_operations.push_back(std::move(op));
            p->it = --inprogess_operations.end();
        }
        else throw SocketError("AcceptEx");
    }
    void AsyncIo::recv(SOCKET sock, void *buffer, size_t len, RecvHandler handler, ErrorHandler error)
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (!start_operation(sock, error)) return;
        Operation::Ptr op(new Recv(sock, buffer, len, handler, error));
        DWORD flags = 0;
        auto ret = WSARecv(sock, &op->buffer, 1, nullptr, &flags, &op->overlapped, nullptr);
        auto err = WSAGetLastError();
        if (!ret || err == WSA_IO_PENDING)
        {
            auto p = op.get();
            inprogess_operations.push_back(std::move(op));
            p->it = --inprogess_operations.end();
        }
        else throw SocketError(err);
    }
    void AsyncIo::send(SOCKET sock, const void *buffer, size_t len, SendHandler handler, ErrorHandler error)
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (!start_operation(sock, error)) return;

        Operation::Ptr op(new Send(sock, buffer, len, handler, error));
        auto ret = WSASend(sock, &op->buffer, 1, nullptr, 0, &op->overlapped, nullptr);
        auto err = WSAGetLastError();
        if (!ret || err == WSA_IO_PENDING)
        {
            auto p = op.get();
            inprogess_operations.push_back(std::move(op));
            p->it = --inprogess_operations.end();
        }
        else throw SocketError(err);
    }
    void AsyncIo::send_all(SOCKET sock, const void *buffer, size_t len, SendHandler handler, ErrorHandler error)
    {
        send_all_next(std::unique_ptr<SendAll, Operation::Deleter>(
            new SendAll(sock, buffer, len, handler, error)), 0);
    }
    void AsyncIo::iocp_loop()
    {
        while (running || !inprogess_operations.empty())
        {
            DWORD bytes;
            ULONG_PTR completion_key;
            OVERLAPPED *overlapped = nullptr;
            auto ret = GetQueuedCompletionStatus(iocp.port, &bytes, &completion_key, &overlapped, INFINITE);
            auto err = GetLastError();
            if (!overlapped)
            {
                if (ret) continue;
                else throw WinError("GetQueuedCompletionStatus failed", err);
            }

            Operation::Ptr op;

            {
                // Take ownership of operation and erase from list
                std::unique_lock<std::mutex> lock(mutex);
                auto *tmp = (Operation*)overlapped;
                assert((void*)tmp == (void*)&tmp->overlapped);
                op = std::move(*tmp->it);
                inprogess_operations.erase(op->it);
            }

            try
            {
                if (!ret)
                {
                    assert(err != S_OK);
                    if (err == ERROR_OPERATION_ABORTED) throw AsyncAborted();
                    else  throw SocketError(err);
                }
                if (op->type == Operation::ACCEPT)
                {
                    auto accept = (Accept*)op.get();
                    sockaddr *local_addr, *remote_addr;
                    int local_addr_len, remote_addr_len;
                    GetAcceptExSockaddrs(
                        accept->accept_buffer,
                        0, accept->addr_len, accept->addr_len,
                        &local_addr, &local_addr_len, &remote_addr, &remote_addr_len);
                    TcpSocket sock(accept->client_sock, remote_addr);
                    accept->client_sock = INVALID_SOCKET;
                    accept->handler(std::move(sock));
                }
                else if (op->type == Operation::RECV)
                {
                    auto recv = (Send*)op.get();
                    recv->handler(bytes);
                }
                else if (op->type == Operation::SEND)
                {
                    auto send = (Send*)op.get();
                    send->handler(bytes);
                }
                else
                {
                    assert(op->type == Operation::SEND_ALL);
                    send_all_next(std::unique_ptr<SendAll, Operation::Deleter>((SendAll*)op.release()), bytes);
                }
            }
            catch (const std::exception &e)
            {
                call_error(e, op->error);
            }
        }
    }
    bool AsyncIo::start_operation(SOCKET sock, ErrorHandler error)
    {
        if (running)
        {
            CreateIoCompletionPort((HANDLE)sock, iocp.port, (ULONG_PTR)sock, 0);
            return true;
        }
        else
        {
            do_abort(error);
            return false;
        }
    }
    void AsyncIo::send_all_next(std::unique_ptr<SendAll, Operation::Deleter> send, size_t sent)
    {
        try
        {
            send->sent += sent;
            assert(send->sent <= send->len);
            if (send->sent == send->len)
            {
                send->handler(send->sent);
            }
            else
            {
                typedef decltype(send->buffer.len) len_t;
                std::unique_lock<std::mutex> lock(mutex);
                if (!start_operation(send->sock, send->error)) return;
                send->buffer.buf += sent;
                send->buffer.len = (len_t)std::min<size_t>(std::numeric_limits<len_t>::max(), send->len - send->sent);
                auto ret = WSASend(send->sock, &send->buffer, 1, nullptr, 0, &send->overlapped, nullptr);
                auto err = WSAGetLastError();
                if (!ret || err == WSA_IO_PENDING)
                {
                    auto p = send.get();
                    inprogess_operations.push_back(std::move(send));
                    p->it = --inprogess_operations.end();
                }
                else throw SocketError(err);
            }
        }
        catch (const std::exception &e)
        {
            call_error(e, send->error);
        }
    }
    #endif

    void AsyncIo::call_error(const std::exception &e, const ErrorHandler &handler)
    {
        (void)e;
        try
        {
            handler();
        }
        catch (const std::exception &e2)
        {
            std::cerr << "Unexpected exception from AsyncIo error handler.\n";
            std::cerr << e2.what();
            std::terminate();
        }
    }
}
