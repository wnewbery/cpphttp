#pragma once
#include "Socket.hpp"
#include "Os.hpp"
#include <cstdint>
namespace http
{
    /**Basic unencrypted TCP stream socket using SOCKET and related system API's.*/
    class TcpSocket : public Socket
    {
    public:
        TcpSocket();
        /**Establish a new client connection. See connect.*/
        TcpSocket(const std::string &host, uint16_t port);
        /**Construct using an existing socket. See set_socket.*/
        TcpSocket(SOCKET socket, const sockaddr *address);
        virtual ~TcpSocket();

        TcpSocket(const TcpSocket&) = delete;
        TcpSocket& operator= (const TcpSocket&) = delete;

        TcpSocket(TcpSocket &&mv);
        TcpSocket& operator = (TcpSocket &&mv);

        explicit operator bool ()const { return socket != INVALID_SOCKET; }

        /**Take ownership of an existing TCP SOCKET object.
         * The remote address object is used to populate the host and port.
         */
        void set_socket(SOCKET socket, const sockaddr *address);
        /**Create a new client side connection to a remote host or port.
         * Host can either be a hostname or an IP address.
         */
        void connect(const std::string &host, uint16_t port);

        virtual SOCKET get()override { return socket; }
        /**Gets the remote address as a string in the form `host() + ':' + port()`.*/
        virtual std::string address_str()const override;
        /**Get the remote host name or IP address.*/
        const std::string &host()const { return _host; }
        /**Get the remote port number.*/
        uint16_t port()const { return _port;; }
        virtual void close()override;
        virtual void disconnect()override;
        virtual size_t recv(void *buffer, size_t len)override;
        virtual size_t send(const void *buffer, size_t len)override;
        virtual bool check_recv_disconnect()override;
    private:
        SOCKET socket;
        std::string _host;
        uint16_t _port;
    };
}
