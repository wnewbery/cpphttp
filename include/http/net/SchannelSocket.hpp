#pragma once
#include "Socket.hpp"
#include "TcpSocket.hpp"
#include "Os.hpp"
#include <cstdint>
#include <vector>
#include <memory>
namespace http
{
    class SchannelSocket : public Socket
    {
    public:
        SchannelSocket();
        SchannelSocket(const std::string &host, uint16_t port);
        SchannelSocket(SOCKET socket, const sockaddr *address);
        virtual ~SchannelSocket() {}

        void set_socket(SOCKET socket, const sockaddr *address);
        void connect(const std::string &host, uint16_t port);

        virtual std::string address_str()const override;
        virtual void disconnect()override;
        virtual size_t recv(void *buffer, size_t len)override;
        virtual size_t send(const void *buffer, size_t len)override;
    private:
        TcpSocket tcp;

        CtxtHandle context;
        CredHandle credentials;
        std::vector<uint8_t> recv_encrypted_buffer;
        std::vector<uint8_t> recv_decrypted_buffer;
        SecPkgContext_StreamSizes sec_sizes;
        std::unique_ptr<uint8_t[]> header_buffer;
        std::unique_ptr<uint8_t[]> trailer_buffer;

        void clear_schannel_handles();
        void init_credentials();
        void client_handshake_loop(bool initial_read);
        void client_handshake();
        void send_sec_buffers(const SecBufferDesc &buffers);
        /**recv some data into recv_encrypted_buffer*/
        bool recv_encrypted();
    };
}
