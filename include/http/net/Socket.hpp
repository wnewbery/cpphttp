#pragma once
#include <string>
namespace http
{
    class Socket
    {
    public:
        Socket() {}
        virtual ~Socket() {}

        virtual std::string address_str()const = 0;
        virtual void disconnect() = 0;
        virtual size_t recv(void *buffer, size_t len) = 0;
        virtual size_t send(const void *buffer, size_t len) = 0;
        /**See if the socket is ready to recv, and if that recv returns 0.
         * If recv reads some data, that is considered an error.
         * 
         * Intended to be used to see if the remote closed the connection in a
         * situation where no data would be valid.
         */
        virtual bool check_recv_disconnect() = 0;

        void send_all(const void *buffer, size_t len);
    private:
        Socket(const Socket &socket);
        Socket& operator = (const Socket &socket);
    };
}
