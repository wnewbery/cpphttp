#pragma once
#include "Socket.hpp"
#include "TcpSocket.hpp"
#include "Os.hpp"
#include <cstdint>
#include <vector>
#include <memory>
namespace http
{
    /**TLS secure socket using the Window's Secure Channel.*/
    class SchannelSocket : public Socket
    {
    public:
        SchannelSocket();
        /**Establish a client connection to a specific host and port.*/
        SchannelSocket(const std::string &host, uint16_t port);
        /**Construct by taking ownership of an existing socket.*/
        SchannelSocket(SOCKET socket, const sockaddr *address);
        virtual ~SchannelSocket() {}

        /**Construct by taking ownership of an existing socket.*/
        void set_socket(SOCKET socket, const sockaddr *address);
        /**Establish a client connection to a specific host and port.*/
        void connect(const std::string &host, uint16_t port);

        virtual std::string address_str()const override;
        virtual void disconnect()override;
        virtual bool check_recv_disconnect()override;
        virtual size_t recv(void *buffer, size_t len)override;
        virtual size_t send(const void *buffer, size_t len)override;
    private:
        struct UniqueCtxtHandle
        {
            UniqueCtxtHandle();
            ~UniqueCtxtHandle();
            void reset();
            CtxtHandle handle;
        };
        struct UniqueCredHandle
        {
            UniqueCredHandle();
            ~UniqueCredHandle();
            void reset();
            CredHandle handle;
        };

        TcpSocket tcp;

        UniqueCtxtHandle context;
        UniqueCredHandle credentials;
        std::vector<uint8_t> recv_encrypted_buffer;
        std::vector<uint8_t> recv_decrypted_buffer;
        SecPkgContext_StreamSizes sec_sizes;
        std::unique_ptr<uint8_t[]> header_buffer;
        std::unique_ptr<uint8_t[]> trailer_buffer;

        void init_credentials();
        void client_handshake_loop(bool initial_read);
        void client_handshake();
        void send_sec_buffers(const SecBufferDesc &buffers);
        /**recv some data into recv_encrypted_buffer*/
        bool recv_encrypted();
    };
}
