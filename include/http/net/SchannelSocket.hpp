#pragma once
#include "Socket.hpp"
#include "TcpSocket.hpp"
#include "Os.hpp"
#include <cstdint>
#include <vector>
#include <memory>
namespace http
{
    class PrivateCert;
    /**TLS secure socket using the Window's Secure Channel.*/
    class SchannelSocket : public Socket
    {
    public:
        SchannelSocket();
        SchannelSocket(SchannelSocket &&mv);
        /**Establish a client connection to a specific host and port.*/
        SchannelSocket(const std::string &host, uint16_t port);
        /**Construct by taking ownership of an existing socket.*/
        SchannelSocket(SOCKET socket, const sockaddr *address);
        /**Construct by taking ownership of an existing socket.*/
        explicit SchannelSocket(TcpSocket &&socket)
            : SchannelSocket()
        {
            tcp = std::move(socket);
        }
        virtual ~SchannelSocket() {}
        SchannelSocket& operator = (SchannelSocket &&mv);

        /**Construct by taking ownership of an existing socket.*/
        void set_socket(SOCKET socket, const sockaddr *address);
        /**Establish a client connection to a specific host and port.*/
        void connect(const std::string &host, uint16_t port);

        virtual SOCKET get()override { return tcp.get(); }
        virtual std::string address_str()const override;
        virtual void close()override;
        virtual void disconnect()override;
        virtual bool check_recv_disconnect()override;
        virtual bool recv_pending()const override
        {
            return !recv_encrypted_buffer.empty() || !recv_decrypted_buffer.empty();
        }
        virtual size_t recv(void *buffer, size_t len)override;
        virtual size_t send(const void *buffer, size_t len)override;

        virtual void async_disconnect(AsyncIo &aio,
            std::function<void()> handler, AsyncIo::ErrorHandler error)override;
        virtual void async_recv(AsyncIo &aio, void *buffer, size_t len,
            AsyncIo::RecvHandler handler, AsyncIo::ErrorHandler error)override;
        virtual void async_send(AsyncIo &aio, const void *buffer, size_t len,
            AsyncIo::SendHandler handler, AsyncIo::ErrorHandler error)override;
        virtual void async_send_all(AsyncIo &aio, const void *buffer, size_t len,
            AsyncIo::SendHandler handler, AsyncIo::ErrorHandler error)override;
    protected:
        struct UniqueCtxtHandle
        {
            UniqueCtxtHandle();
            UniqueCtxtHandle(UniqueCtxtHandle &&mv);
            ~UniqueCtxtHandle();
            UniqueCtxtHandle& operator = (UniqueCtxtHandle &&mv);
            explicit operator bool()const;
            void reset();
            CtxtHandle handle;
        };
        struct UniqueCredHandle
        {
            UniqueCredHandle();
            UniqueCredHandle(UniqueCredHandle &&mv);
            ~UniqueCredHandle();
            UniqueCredHandle& operator = (UniqueCredHandle &&mv);
            void reset();
            CredHandle handle;
        };
        struct SecBufferSingleAutoFree
        {
            SecBuffer buffer;
            SecBufferDesc desc;

            SecBufferSingleAutoFree()
                : buffer{0, SECBUFFER_TOKEN, nullptr}
                , desc{SECBUFFER_VERSION, 1, &buffer}
            {
            }
            ~SecBufferSingleAutoFree()
            {
                free();
            }
            void free();
        };

        TcpSocket tcp;
        bool server;

        UniqueCtxtHandle context;
        UniqueCredHandle credentials;
        std::vector<uint8_t> recv_encrypted_buffer;
        std::vector<uint8_t> recv_decrypted_buffer;
        SecPkgContext_StreamSizes sec_sizes;
        std::unique_ptr<uint8_t[]> header_buffer;
        std::unique_ptr<uint8_t[]> trailer_buffer;

        /**Allocates header_buffer and trailer_buffer according to QueryContextAttributes*/
        void alloc_buffers();
        void init_credentials();
        void client_handshake_loop(bool initial_read);
        void client_handshake();
        void send_sec_buffers(const SecBufferDesc &buffers);
        /**Read data from local recv_decrypted_buffer if possible.*/
        size_t recv_cached(void *vbytes, size_t len);
        /**recv some data into recv_encrypted_buffer*/
        bool recv_encrypted();
        /**Decrypts some data from recv_encrypted_buffer into a specified buffer and overflow into
         * recv_decrypted_buffer.
         * @return true if data was decrypted, false if more input is needed.
         */
        bool decrypt(void *buffer, size_t len, size_t *out_len);
        void async_send_all_next(AsyncIo &aio, const char *buffer, size_t len, size_t sent,
            AsyncIo::SendHandler handler, AsyncIo::ErrorHandler error);

        /**Creates the message for disconnect and async_disconnect.*/
        void disconnect_message(SecBufferSingleAutoFree &buffer);
    };

    /**Server side S-Channel socket. Presents a certificate on connection.*/
    class SchannelServerSocket : public SchannelSocket
    {
    public:
        /**Construct by taking ownership of an existing socket.*/
        SchannelServerSocket() {}
        SchannelServerSocket(TcpSocket &&socket, const PrivateCert &cert);
        SchannelServerSocket(SchannelServerSocket &&mv) = default;
        SchannelServerSocket& operator = (SchannelServerSocket &&mv) = default;
    protected:
        void tls_accept(const PrivateCert &cert);
        void server_handshake(const PrivateCert &cert);
        void server_handshake_loop();
    };
}
