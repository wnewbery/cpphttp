#include "net/AsyncIo.hpp"
#include "net/Net.hpp"
#include "net/TcpSocket.hpp"
#include "net/TcpListenSocket.hpp"
#include <algorithm>
#include <limits>
#include <cassert>

namespace http
{
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
                    catch (const std::exception &)
                    {
                        op.error();
                    }
                }
                ++i;
            }
        }

        // Abort
        {
            std::unique_lock<std::mutex> lock(mutex);
            for (auto &i : in_progress.accept) do_abort(i);
            for (auto &i : in_progress.recv) for (auto &j : i.second) do_abort(j);
            for (auto &i : in_progress.send) for (auto &j : i.second) do_abort(j);
            for (auto &i : new_operations.accept) do_abort(i);
            for (auto &i : new_operations.recv) do_abort(i);
            for (auto &i : new_operations.send) do_abort(i);

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
}
