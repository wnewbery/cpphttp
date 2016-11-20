#pragma once
#include "Os.hpp"
#include <string>
namespace http
{
    /**Base socket type.
     * Different underlying protocols may potentionally be used, e.g. TCP/IP or TLS, but it will
     * always be a stream socket.
     *
     * Socket objects generally own the underlying socket, and will close it when the object is
     * destroyed in the destructor.
     */
    class Socket
    {
    public:
        Socket() {}
        Socket(Socket&&)=default;
        virtual ~Socket() {}
        Socket& operator = (Socket&&)=default;

        /**Get the underlaying socket.*/
        virtual SOCKET get() = 0;
        /**Gets the remote address as a string for display purposes.
         * Includes the port number and may use a hostname or ip address.
         */
        virtual std::string address_str()const = 0;
        /**Close the socket, without waiting for a clean disconnect.*/
        virtual void close() = 0;
        /**Disconnect this socket if connected. Sending and receiving will fail after.*/
        virtual void disconnect() = 0;
        /**True if the socket has locally cached data pending recv.
         * Because this data is locally cached, and not pending OS data, it will not trigger
         * a read on select() or similar low-level network API's.
         */
        virtual bool recv_pending()const { return false; }
        /**Receive up to len bytes. Works like Berkeley `recv`.*/
        virtual size_t recv(void *buffer, size_t len) = 0;
        /**Send up to len bytes. Works like Berkeley `send`.*/
        virtual size_t send(const void *buffer, size_t len) = 0;
        /**See if the socket is ready to recv, and if that recv returns 0.
         * If recv reads some data, that is considered an error.
         *
         * Intended to be used to see if the remote closed the connection in a
         * situation where no data would be valid.
         */
        virtual bool check_recv_disconnect() = 0;
        /**Sends len bytes, calling send repeatedly if needed.*/
        void send_all(const void *buffer, size_t len);
    private:
        Socket(const Socket &socket);
        Socket& operator = (const Socket &socket);
    };
}
