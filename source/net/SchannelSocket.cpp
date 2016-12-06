#include "net/SchannelSocket.hpp"
#include "net/Net.hpp"
#include "net/Schannel.hpp"
#include "String.hpp"
#include <cassert>
#include <algorithm>

namespace http
{
    using namespace detail;
    namespace
    {
        static const DWORD SSPI_FLAGS = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT |
            ISC_REQ_CONFIDENTIALITY | ISC_RET_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY |
            ISC_REQ_STREAM;
    }

    SchannelSocket::UniqueCtxtHandle::UniqueCtxtHandle()
    {
        SecInvalidateHandle(&handle);
    }
    SchannelSocket::UniqueCtxtHandle::UniqueCtxtHandle(UniqueCtxtHandle &&mv)
        : handle(mv.handle)
    {
        SecInvalidateHandle(&mv.handle);
    }
    SchannelSocket::UniqueCtxtHandle::~UniqueCtxtHandle()
    {
        reset();
    }
    SchannelSocket::UniqueCtxtHandle& SchannelSocket::UniqueCtxtHandle::operator = (UniqueCtxtHandle &&mv)
    {
        std::swap(handle, mv.handle);
        return *this;
    }
    SchannelSocket::UniqueCtxtHandle::operator bool()const
    {
        return SecIsValidHandle(&handle);
    }
    void SchannelSocket::UniqueCtxtHandle::reset()
    {
        if (SecIsValidHandle(&handle))
        {
            sspi->DeleteSecurityContext(&handle);
            SecInvalidateHandle(&handle);
        }
    }

    SchannelSocket::UniqueCredHandle::UniqueCredHandle()
    {
        SecInvalidateHandle(&handle);
    }
    SchannelSocket::UniqueCredHandle::UniqueCredHandle(UniqueCredHandle &&mv)
        : handle(mv.handle)
    {
        SecInvalidateHandle(&mv.handle);
    }
    SchannelSocket::UniqueCredHandle::~UniqueCredHandle()
    {
        reset();
    }
    SchannelSocket::UniqueCredHandle& SchannelSocket::UniqueCredHandle::operator = (UniqueCredHandle &&mv)
    {
        std::swap(handle, mv.handle);
        return *this;
    }
    void SchannelSocket::UniqueCredHandle::reset()
    {
        if (SecIsValidHandle(&handle))
        {
            sspi->FreeCredentialsHandle(&handle);
            SecInvalidateHandle(&handle);
        }
    }
    void SchannelSocket::SecBufferSingleAutoFree::free()
    {
        if (buffer.pvBuffer) sspi->FreeContextBuffer(buffer.pvBuffer);
        buffer.pvBuffer = nullptr;
        buffer.cbBuffer = 0;
    }

    SchannelSocket::SchannelSocket()
        : tcp(), server(false)
        , context(), credentials()
        , recv_encrypted_buffer(), recv_decrypted_buffer()
        , sec_sizes()
        , header_buffer(nullptr), trailer_buffer(nullptr)
    {
    }
    SchannelSocket::SchannelSocket(SchannelSocket &&mv)
    {
        *this = std::move(mv);
    }
    SchannelSocket::SchannelSocket(const std::string & host, uint16_t port)
        : SchannelSocket()
    {
        connect(host, port);
    }
    SchannelSocket::SchannelSocket(SOCKET socket, const sockaddr * address)
        : SchannelSocket()
    {
        set_socket(socket, address);
    }
    SchannelSocket& SchannelSocket::operator = (SchannelSocket &&mv)
    {
        tcp = std::move(mv.tcp);
        context = std::move(mv.context);
        credentials = std::move(mv.credentials);
        recv_encrypted_buffer = std::move(mv.recv_encrypted_buffer);
        recv_decrypted_buffer = std::move(mv.recv_decrypted_buffer);
        sec_sizes = std::move(mv.sec_sizes);
        header_buffer = std::move(mv.header_buffer);
        trailer_buffer = std::move(mv.trailer_buffer);
        return *this;
    }
    void SchannelSocket::set_socket(SOCKET socket, const sockaddr * address)
    {
        tcp.set_socket(socket, address);
    }
    void SchannelSocket::connect(const std::string & host, uint16_t port)
    {
        assert(sspi);

        context.reset();
        credentials.reset();
        recv_encrypted_buffer.clear();
        recv_decrypted_buffer.clear();
        tcp.connect(host, port);

        client_handshake();
        alloc_buffers();
    }
    std::string SchannelSocket::address_str() const
    {
        return tcp.address_str();
    }
    bool SchannelSocket::check_recv_disconnect()
    {
        return tcp.check_recv_disconnect();
    }
    void SchannelSocket::close()
    {
        tcp.close();
        context.reset();
        credentials.reset();
    }

    void SchannelSocket::disconnect_message(SecBufferSingleAutoFree &buffer)
    {
        // Apply SCHANNEL_SHUTDOWN
        DWORD type = SCHANNEL_SHUTDOWN;
        SecBuffer out_buffer = { sizeof(type), SECBUFFER_TOKEN, &type };
        SecBufferDesc out_buffer_desc = { SECBUFFER_VERSION, 1, &out_buffer };
        auto status = sspi->ApplyControlToken(&context.handle, &out_buffer_desc);
        if (status != SEC_E_OK) throw std::runtime_error("ApplyControlToken failed");

        // Create a tls close notification message
        TimeStamp expiry;
        DWORD sspi_out_flags;

        if (server)
        {
            DWORD sspi_flags =
                ASC_REQ_SEQUENCE_DETECT |
                ASC_REQ_REPLAY_DETECT |
                ASC_REQ_CONFIDENTIALITY |
                ASC_REQ_EXTENDED_ERROR |
                ASC_REQ_ALLOCATE_MEMORY |
                ASC_REQ_STREAM;
            status = sspi->AcceptSecurityContext(&credentials.handle, &context.handle, nullptr,
                sspi_flags, 0, nullptr, &buffer.desc, &sspi_out_flags, &expiry);
            if (status != SEC_E_OK) throw std::runtime_error("AcceptSecurityContext failed");
        }
        else
        {
            status = sspi->InitializeSecurityContextW(
                &credentials.handle, &context.handle, nullptr, SSPI_FLAGS, 0,
                SECURITY_NATIVE_DREP, nullptr, 0, &context.handle, &buffer.desc,
                &sspi_out_flags, &expiry);
            if (status != SEC_E_OK) throw std::runtime_error("InitializeSecurityContext failed");

        }
    }
    void SchannelSocket::disconnect()
    {
        SecBufferSingleAutoFree notify_buffer;
        disconnect_message(notify_buffer);

        // Send the message
        if (notify_buffer.buffer.pvBuffer && notify_buffer.buffer.cbBuffer)
        {
            tcp.send_all(notify_buffer.buffer.pvBuffer, notify_buffer.buffer.cbBuffer);
        }

        // Cleanup
        context.reset();
        credentials.reset();
        tcp.disconnect();
    }
    void SchannelSocket::async_disconnect(AsyncIo &aio,
        std::function<void()> handler, AsyncIo::ErrorHandler error)
    {
        try
        {
            auto notify_buffer = std::make_shared<SecBufferSingleAutoFree>();
            disconnect_message(*notify_buffer);

            tcp.async_send_all(aio, notify_buffer->buffer.pvBuffer, notify_buffer->buffer.cbBuffer,
                [notify_buffer, handler](size_t) { handler(); },
                [notify_buffer, error]() { error(); });
        }
        catch (const std::exception &)
        {
            error();
        }
    }

    size_t SchannelSocket::recv_cached(void *vbytes, size_t len)
    {
        if (recv_decrypted_buffer.empty()) return 0;
        // Already had some data (last recv was less than SSPI decrypted)
        auto len2 = std::min(len, recv_decrypted_buffer.size());
        memcpy(vbytes, recv_decrypted_buffer.data(), len2);
        recv_decrypted_buffer.erase(recv_decrypted_buffer.begin(), recv_decrypted_buffer.begin() + len2);
        return len2;
    }
    bool SchannelSocket::decrypt(void *buffer, size_t len, size_t *out_len)
    {
        if (recv_encrypted_buffer.empty()) return false;

        *out_len = 0;
        while (true)
        {
            // Prepare decryption buffers
            SecBuffer buffers[4];
            buffers[0] = { (DWORD)recv_encrypted_buffer.size(), SECBUFFER_DATA, &recv_encrypted_buffer[0] };
            buffers[1] = { 0, SECBUFFER_EMPTY, nullptr };
            buffers[2] = { 0, SECBUFFER_EMPTY, nullptr };
            buffers[3] = { 0, SECBUFFER_EMPTY, nullptr };
            SecBufferDesc buffers_desc = { SECBUFFER_VERSION, 4, buffers };

            // Decrypt
            auto status = sspi->DecryptMessage(&context.handle, &buffers_desc, 0, nullptr);

            if (!server && status == SEC_I_CONTEXT_EXPIRED)
            {
                return true; // Disconnect signal
            }
            else if (status == SEC_E_OK || status == SEC_I_RENEGOTIATE)
            {
                SecBuffer *buffer_data = nullptr, *buffer_extra = nullptr;
                for (int i = 0; i < 4; ++i)
                {
                    if (!buffer_data && buffers[i].BufferType == SECBUFFER_DATA) buffer_data = &buffers[i];
                    if (!buffer_extra && buffers[i].BufferType == SECBUFFER_EXTRA) buffer_extra = &buffers[i];
                }

                if (buffer_data) //Decrypted data
                {
                    auto decrypt_len = (size_t)buffer_data->cbBuffer;
                    auto decrypt_p = (uint8_t*)buffer_data->pvBuffer;
                    auto out_cpy_len = std::min(decrypt_len, len);

                    memcpy((char*)buffer, decrypt_p, out_cpy_len);
                    *out_len = out_cpy_len;

                    if (out_cpy_len < decrypt_len)
                    {   // Had more data than can output, store for later
                        recv_decrypted_buffer.insert(recv_decrypted_buffer.end(),
                            decrypt_p + out_cpy_len, decrypt_p + decrypt_len);
                    }
                }

                //Handle clearing consumed data, and saving extra data in recv_encrypted_buffer
                if (buffer_extra) //extra input data
                {
                    recv_encrypted_buffer.erase(
                        recv_encrypted_buffer.begin(),
                        recv_encrypted_buffer.begin() + recv_encrypted_buffer.size() - buffer_extra->cbBuffer);
                }
                else recv_encrypted_buffer.clear();

                if (status == SEC_I_RENEGOTIATE)
                {
                    if (server)
                        throw std::runtime_error("Unexpected server side SEC_I_RENEGOTIATE");

                    client_handshake_loop(false);
                }

                if (buffer_data) return true;
            }
            else if (status == SEC_E_INCOMPLETE_MESSAGE)
            {
                //Read more data
                return false;
            }
            else
            {
                throw NetworkError("DecryptMessage failed");
            }
        }
    }
    size_t SchannelSocket::recv(void *buffer, size_t len)
    {
        if (auto len2 = recv_cached(buffer, len)) return len2;

        size_t len_out = 0;
        while (!decrypt(buffer, len, &len_out))
        {
            if (!recv_encrypted())
            {
                if (recv_encrypted_buffer.empty()) break; //remote disconnected
                else throw NetworkError("Unexpected socket disconnect");
            }
        }
        return len_out;
    }
    void SchannelSocket::async_recv(AsyncIo &aio, void *buffer, size_t len,
        AsyncIo::RecvHandler handler, AsyncIo::ErrorHandler error)
    {
        if (auto len2 = recv_cached(buffer, len)) return handler(len2);

        struct Processor
        {
            SchannelSocket *sock;
            AsyncIo &aio;
            void *buffer;
            size_t len;
            AsyncIo::RecvHandler handler;
            AsyncIo::ErrorHandler error;

            void operator()()
            {
                try
                {
                    size_t out = 0;
                    if (sock->decrypt(buffer, len, &out))
                    {
                        handler(out);
                        delete this;
                    }
                    else
                    {
                        auto p = sock->recv_encrypted_buffer.size();
                        sock->recv_encrypted_buffer.resize(p + 4096);
                        aio.recv(sock->tcp.get(), sock->recv_encrypted_buffer.data() + p, 4096,
                            [this, p](size_t len)
                            {
                                sock->recv_encrypted_buffer.resize(p + len);
                                (*this)();
                            },
                            [this, p]()
                            {
                                sock->recv_encrypted_buffer.resize(p);
                                error();
                                delete this;
                            });
                    }
                }
                catch (const std::exception&)
                {
                    error();
                    delete this;
                }
            }
        };
        auto processor = new Processor{ this, aio, buffer, len, handler, error };
        (*processor)();
    }
    size_t SchannelSocket::send(const void * buffer, size_t len)
    {
        if (len > sec_sizes.cbMaximumMessage) len = sec_sizes.cbMaximumMessage;
        // Prepare encryption buffers
        std::vector<uint8_t> bytes_tmp((const char*)buffer, ((const char*)buffer) + len);
        SecBuffer buffers[4];
        buffers[0] = { sec_sizes.cbHeader, SECBUFFER_STREAM_HEADER, header_buffer.get() };
        buffers[1] = { (DWORD)len, SECBUFFER_DATA, &bytes_tmp[0] };
        buffers[2] = { sec_sizes.cbTrailer, SECBUFFER_STREAM_TRAILER, trailer_buffer.get() };
        buffers[3] = { 0, SECBUFFER_EMPTY, nullptr };
        SecBufferDesc buffers_desc = { SECBUFFER_VERSION, 4, buffers };
        // Encrypt data
        auto status = sspi->EncryptMessage(&context.handle, 0, &buffers_desc, 0);
        if (FAILED(status)) throw NetworkError("EncryptMessage failed");
        // Send data
        send_sec_buffers(buffers_desc);

        return len;
    }
    void SchannelSocket::async_send(AsyncIo &aio, const void *raw_buffer, size_t len,
        AsyncIo::SendHandler handler, AsyncIo::ErrorHandler error)
    {
        auto header = sec_sizes.cbHeader;
        auto trailer = sec_sizes.cbTrailer;
        auto max = sec_sizes.cbMaximumMessage;

        auto blocks = (len + max - 1) / max;
        std::shared_ptr<char> buffer(new char[(header + trailer) * blocks + len]);
        auto buffer_p = buffer.get();
        auto raw_p = (const char*)raw_buffer;
        for (size_t i = 0; i < blocks; ++i)
        {
            DWORD block_len = i < blocks - 1 ? max : (DWORD)(len % max);
            memcpy(buffer_p + header, raw_p, block_len);

            SecBuffer buffers[4];
            buffers[0] = { header, SECBUFFER_STREAM_HEADER, buffer_p };
            buffers[1] = { (DWORD)len, SECBUFFER_DATA, buffer_p + header };
            buffers[2] = { trailer, SECBUFFER_STREAM_TRAILER, buffer_p + header + block_len };
            buffers[3] = { 0, SECBUFFER_EMPTY, nullptr };
            SecBufferDesc buffers_desc = { SECBUFFER_VERSION, 4, buffers };

            // Encrypt data
            auto status = sspi->EncryptMessage(&context.handle, 0, &buffers_desc, 0);
            if (FAILED(status)) throw NetworkError("EncryptMessage failed");
            assert(buffers[0].cbBuffer == header);
            assert(buffers[1].cbBuffer == block_len);
            assert(buffers[2].cbBuffer <= trailer);
            assert(buffers[3].BufferType == SECBUFFER_EMPTY);

            raw_p += block_len;
            buffer_p += header + block_len + buffers[2].cbBuffer;
        }

        // Send data
        tcp.async_send_all(aio, buffer.get(), (size_t)(buffer_p - buffer.get()),
            [buffer, handler, len](size_t) { handler(len); },
            [buffer, error]() { error(); });
    }
    void SchannelSocket::async_send_all(AsyncIo &aio, const void *buffer, size_t len,
        AsyncIo::SendHandler handler, AsyncIo::ErrorHandler error)
    {
        async_send(aio, buffer, len, handler, error);
    }

    void SchannelSocket::alloc_buffers()
    {
        auto status = sspi->QueryContextAttributes(&context.handle, SECPKG_ATTR_STREAM_SIZES, &sec_sizes);
        if (FAILED(status)) throw NetworkError("QueryContextAttributes SECPKG_ATTR_STREAM_SIZES failed");

        header_buffer.reset(new uint8_t[sec_sizes.cbHeader]);
        trailer_buffer.reset(new uint8_t[sec_sizes.cbTrailer]);
    }
    void SchannelSocket::init_credentials()
    {
        SCHANNEL_CRED schannel_cred = { 0 };
        schannel_cred.dwVersion = SCHANNEL_CRED_VERSION;
        schannel_cred.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT;
        schannel_cred.dwFlags |= SCH_CRED_AUTO_CRED_VALIDATION;
        schannel_cred.dwFlags |= SCH_CRED_NO_DEFAULT_CREDS;
        schannel_cred.dwFlags |= SCH_USE_STRONG_CRYPTO;

        TimeStamp expiry;
        credentials.reset();
        auto status = sspi->AcquireCredentialsHandleW(nullptr, UNISP_NAME_W, SECPKG_CRED_OUTBOUND,
            nullptr, &schannel_cred, nullptr, nullptr, &credentials.handle, &expiry);

        if (status != SEC_E_OK) throw std::runtime_error("AcquireCredentialsHandleW failed");
    }

    void SchannelSocket::client_handshake_loop(bool initial_read)
    {
        bool do_read = initial_read;
        SecBuffer in_buffers[2];
        SecBufferDesc in_buffer_desc = { SECBUFFER_VERSION, 2, in_buffers };
        SecBufferSingleAutoFree out_buffer;
        TimeStamp expiry;
        SECURITY_STATUS status = SEC_I_CONTINUE_NEEDED;

        while (true)
        {
            // Read data if needed
            if (recv_encrypted_buffer.empty() || status == SEC_E_INCOMPLETE_MESSAGE)
            {
                if (do_read) recv_encrypted();
                else do_read = true;
            }

            // Set up buffers
            in_buffers[0] = { (DWORD)recv_encrypted_buffer.size(), SECBUFFER_TOKEN, recv_encrypted_buffer.data() };
            in_buffers[1] = { 0, SECBUFFER_EMPTY, nullptr };
            out_buffer.buffer = { 0, SECBUFFER_TOKEN, nullptr };

            // Call SSPI
            DWORD sspi_out_flags;
            status = sspi->InitializeSecurityContextW(&credentials.handle, &context.handle, nullptr,
                SSPI_FLAGS, 0, SECURITY_NATIVE_DREP, &in_buffer_desc, 0, nullptr,
                &out_buffer.desc, &sspi_out_flags, &expiry);

            // Send requested output
            if (status == SEC_E_OK || status == SEC_I_CONTINUE_NEEDED ||
                (FAILED(status) && (sspi_out_flags & ISC_RET_EXTENDED_ERROR)))
            {
                if (out_buffer.buffer.cbBuffer && out_buffer.buffer.pvBuffer)
                {
                    send_sec_buffers(out_buffer.desc);
                    out_buffer.free();
                }
            }
            // Read more data and retry
            if (status == SEC_E_INCOMPLETE_MESSAGE) continue;
            //completed
            else if (status == SEC_E_OK)
            {
                //Might have read too much data before, so store for later
                if (in_buffers[1].BufferType == SECBUFFER_EXTRA)
                {
                    auto &buf = in_buffers[1];
                    auto p = (uint8_t*)out_buffer.buffer.pvBuffer;
                    assert(recv_encrypted_buffer.size() >= buf.cbBuffer);
                    assert(!buf.cbBuffer || p >= recv_encrypted_buffer.data() && p < recv_encrypted_buffer.data() + recv_encrypted_buffer.size());

                    recv_encrypted_buffer.erase(
                        recv_encrypted_buffer.begin(),
                        recv_encrypted_buffer.begin() + recv_encrypted_buffer.size() - buf.cbBuffer);
                }
                else recv_encrypted_buffer.clear();
                break;
            }
            //fatal error
            else if (FAILED(status))
            {
                switch (status)
                {
                case SEC_E_WRONG_PRINCIPAL:
                case SEC_E_UNTRUSTED_ROOT:
                case SEC_E_CERT_EXPIRED:
                    throw CertificateVerificationError(tcp.host(), tcp.port());
                default:
                    throw ConnectionError("TLS handshake failed with " + std::to_string(status), tcp.host(), tcp.port());
                }
            }
            //server wants credentials, but not supported
            else if (status == SEC_I_INCOMPLETE_CREDENTIALS)
            {
                throw ConnectionError("TLS server requested client certificate, which is not supported", tcp.host(), tcp.port());
            }
            //Move any spare input to the start of the buffer then loop
            else
            {
                assert(recv_encrypted_buffer.size() >= in_buffers[1].cbBuffer);
                recv_encrypted_buffer.erase(
                    recv_encrypted_buffer.begin(),
                    recv_encrypted_buffer.begin() + recv_encrypted_buffer.size() - in_buffers[1].cbBuffer);
            }
        }
    }

    void SchannelSocket::client_handshake()
    {
        init_credentials();

        SecBufferSingleAutoFree out_buffer;
        DWORD sspi_out_flags;
        TimeStamp expiry;
        SECURITY_STATUS status;
        auto host_16 = utf8_to_utf16(tcp.host());

        out_buffer.buffer = { 0, SECBUFFER_TOKEN, nullptr };
        //initialize
        context.reset();
        status = sspi->InitializeSecurityContextW(&credentials.handle, nullptr, &host_16[0], SSPI_FLAGS,
            0, SECURITY_NATIVE_DREP, nullptr, 0, &context.handle, &out_buffer.desc,
            &sspi_out_flags, &expiry);
        if (status != SEC_I_CONTINUE_NEEDED) throw NetworkError("InitializeSecurityContextW failed");

        //send initial data to server
        send_sec_buffers(out_buffer.desc);

        client_handshake_loop(true);
    }

    void SchannelSocket::send_sec_buffers(const SecBufferDesc &buffers)
    {
        for (DWORD i = 0; i < buffers.cBuffers; ++i)
        {
            auto &buf = buffers.pBuffers[i];
            tcp.send_all((const uint8_t*)buf.pvBuffer, buf.cbBuffer);
        }
    }

    bool SchannelSocket::recv_encrypted()
    {
        uint8_t buf[4096];
        size_t len = tcp.recv(buf, sizeof(buf));
        recv_encrypted_buffer.insert(recv_encrypted_buffer.end(), buf, buf + len);
        return len > 0;
    }
}
