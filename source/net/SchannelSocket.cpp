#include "net/SchannelSocket.hpp"
#include "net/Net.hpp"
#include "String.hpp"
#include <cassert>
#include <algorithm>

namespace http
{
    extern SecurityFunctionTableW *sspi; //http::init_net, Net.cpp

    namespace
    {
        static const DWORD SSPI_FLAGS = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT |
            ISC_REQ_CONFIDENTIALITY | ISC_RET_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY |
            ISC_REQ_STREAM;

        struct SecBufferSingleAutoFree
        {
            SecBuffer buffer;
            SecBufferDesc desc;

            SecBufferSingleAutoFree()
                : buffer{ 0, SECBUFFER_TOKEN, nullptr }
                , desc{ SECBUFFER_VERSION, 1, &buffer }
            {
            }
            ~SecBufferSingleAutoFree()
            {
                if (buffer.pvBuffer) sspi->FreeContextBuffer(buffer.pvBuffer);
            }
        };
    }

    SchannelSocket::UniqueCtxtHandle::UniqueCtxtHandle()
    {
        SecInvalidateHandle(&handle);
    }
    SchannelSocket::UniqueCtxtHandle::~UniqueCtxtHandle()
    {
        reset();
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
    SchannelSocket::UniqueCredHandle::~UniqueCredHandle()
    {
        reset();
    }
    void SchannelSocket::UniqueCredHandle::reset()
    {
        if (SecIsValidHandle(&handle))
        {
            sspi->FreeCredentialsHandle(&handle);
            SecInvalidateHandle(&handle);
        }
    }

    SchannelSocket::SchannelSocket()
        : tcp()
        , context(), credentials()
        , recv_encrypted_buffer(), recv_decrypted_buffer()
        , sec_sizes()
        , header_buffer(nullptr), trailer_buffer(nullptr)
    {
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

        auto status = sspi->QueryContextAttributes(&context.handle, SECPKG_ATTR_STREAM_SIZES, &sec_sizes);
        if (FAILED(status)) throw NetworkError("QueryContextAttributes SECPKG_ATTR_STREAM_SIZES failed");

        header_buffer.reset(new uint8_t[sec_sizes.cbHeader]);
        trailer_buffer.reset(new uint8_t[sec_sizes.cbTrailer]);
    }
    std::string SchannelSocket::address_str() const
    {
        return tcp.address_str();
    }
    bool SchannelSocket::check_recv_disconnect()
    {
        return tcp.check_recv_disconnect();
    }
    void SchannelSocket::disconnect()
    {
        DWORD type = SCHANNEL_SHUTDOWN;
        SecBuffer out_buffer = { sizeof(type), SECBUFFER_TOKEN, &type };
        SecBufferDesc out_buffer_desc = { SECBUFFER_VERSION, 1, &out_buffer };
        auto status = sspi->ApplyControlToken(&context.handle, &out_buffer_desc);
        if (status != SEC_E_OK) throw std::runtime_error("ApplyControlToken failed");
        //create a tls close notification message
        SecBufferSingleAutoFree notify_buffer;
        TimeStamp expiry;
        DWORD sspi_out_flags;
        status = sspi->InitializeSecurityContextW(&credentials.handle, &context.handle, nullptr, SSPI_FLAGS, 0,
            SECURITY_NATIVE_DREP, nullptr, 0, &context.handle, &notify_buffer.desc,
            &sspi_out_flags, &expiry);
        if (status != SEC_E_OK) throw std::runtime_error("InitializeSecurityContext failed");

        //send the message
        if (notify_buffer.buffer.pvBuffer && notify_buffer.buffer.cbBuffer)
        {
            tcp.send_all((const uint8_t*)notify_buffer.buffer.pvBuffer, notify_buffer.buffer.cbBuffer);
        }

        //cleanup
        context.reset();
        credentials.reset();
        tcp.disconnect();
    }

    size_t SchannelSocket::recv(void * vbytes, size_t len)
    {
        auto bytes = (char*)vbytes;
        if (!recv_decrypted_buffer.empty())
        {   // Already had some data (last recv was less than SSPI decrypted)
            auto len2 = std::min(len, recv_decrypted_buffer.size());
            memcpy(bytes, recv_decrypted_buffer.data(), len2);
            recv_decrypted_buffer.erase(recv_decrypted_buffer.begin(), recv_decrypted_buffer.begin() + len2);
            return len2;
        }
        bool do_read = recv_encrypted_buffer.size() < sec_sizes.cbBlockSize;
        size_t len_out = 0;
        while (true)
        {
            if (do_read)
            {
                if (!recv_encrypted())
                {
                    if (recv_encrypted_buffer.empty()) break; //remote disconnected
                    else throw NetworkError("Unexpected socket disconnect");
                }
            }
            do_read = true;
            //Prepare decryption buffers
            SecBuffer buffers[4];
            buffers[0] = { (DWORD)recv_encrypted_buffer.size(), SECBUFFER_DATA, &recv_encrypted_buffer[0] };
            buffers[1] = { 0, SECBUFFER_EMPTY, nullptr };
            buffers[2] = { 0, SECBUFFER_EMPTY, nullptr };
            buffers[3] = { 0, SECBUFFER_EMPTY, nullptr };
            SecBufferDesc buffers_desc = { SECBUFFER_VERSION, 4, buffers };

            //Decrypt
            auto status = sspi->DecryptMessage(&context.handle, &buffers_desc, 0, nullptr);

            if (status == SEC_I_CONTEXT_EXPIRED) break; //disconnect signal
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
                    auto out_cpy_len = std::min(decrypt_len, len - len_out);

                    memcpy(bytes + len_out, decrypt_p, out_cpy_len);
                    len_out += out_cpy_len;

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
                    client_handshake_loop(false);
                }

                if (buffer_data) break;
            }
            else if (status == SEC_E_INCOMPLETE_MESSAGE)
            {
                //Read more data
                continue;
            }
            else
            {
                throw NetworkError("DescyptMessage failed");
            }
        }

        return len_out;
    }
    size_t SchannelSocket::send(const void * buffer, size_t len)
    {
        //Prepare encryption buffers
        std::vector<uint8_t> bytes_tmp((const char*)buffer, ((const char*)buffer) + len);
        SecBuffer buffers[4];
        buffers[0] = { sec_sizes.cbHeader, SECBUFFER_STREAM_HEADER, header_buffer.get() };
        buffers[1] = { (DWORD)len, SECBUFFER_DATA, &bytes_tmp[0] };
        buffers[2] = { sec_sizes.cbTrailer, SECBUFFER_STREAM_TRAILER, trailer_buffer.get() };
        buffers[3] = { 0, SECBUFFER_EMPTY, nullptr };
        SecBufferDesc buffers_desc = { SECBUFFER_VERSION, 4, buffers };
        //Encrypt data
        auto status = sspi->EncryptMessage(&context.handle, 0, &buffers_desc, 0);
        if (FAILED(status)) throw NetworkError("EncryptMessage failed");
        //Send data
        send_sec_buffers(buffers_desc);

        return len;
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
            //Read data if needed
            if (recv_encrypted_buffer.empty() || status == SEC_E_INCOMPLETE_MESSAGE)
            {
                if (do_read) recv_encrypted();
                else do_read = true;
            }

            //set up buffers
            in_buffers[0] = { (DWORD)recv_encrypted_buffer.size(), SECBUFFER_TOKEN, recv_encrypted_buffer.data() };
            in_buffers[1] = { 0, SECBUFFER_EMPTY, nullptr };
            out_buffer.buffer = { 0, SECBUFFER_TOKEN, nullptr };
            //call sspi
            DWORD sspi_out_flags;
            status = sspi->InitializeSecurityContextW(&credentials.handle, &context.handle, nullptr,
                SSPI_FLAGS, 0, SECURITY_NATIVE_DREP, &in_buffer_desc, 0, nullptr,
                &out_buffer.desc, &sspi_out_flags, &expiry);

            //send requested output
            if (status == SEC_E_OK || status == SEC_I_CONTINUE_NEEDED ||
                (FAILED(status) && (sspi_out_flags & ISC_RET_EXTENDED_ERROR)))
            {
                if (out_buffer.buffer.cbBuffer && out_buffer.buffer.pvBuffer)
                {
                    send_sec_buffers(out_buffer.desc);
                    sspi->FreeContextBuffer(out_buffer.buffer.pvBuffer);
                    out_buffer.buffer.pvBuffer = nullptr;
                    out_buffer.buffer.cbBuffer = 0;
                }
            }

            //read more data and retry
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
                throw NetworkError("TLS handshake failed with " + std::to_string(status));
            }
            //server wants credentials, but not supported
            else if (status == SEC_I_INCOMPLETE_CREDENTIALS)
            {
                throw NetworkError("TLS server requested client certificate, which is not supported");
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
