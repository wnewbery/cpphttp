#include "net/SchannelSocket.hpp"
#include "net/Net.hpp"
#include "net/Cert.hpp"
#include "net/Schannel.hpp"
#include "String.hpp"
#include <cassert>
#include <algorithm>

namespace http
{
    using namespace detail;
    /*namespace
    {
        struct CertContext
        {
            PCCERT_CONTEXT handle;
            explicit CertContext() : handle(NULL) {}
            CertContext(CertContext &&mv) : handle(mv.handle)
            {
                mv.handle = NULL;
            }
            CertContext(const CertContext&) = delete;
            CertContext& operator = (const CertContext&) = delete;
            CertContext& operator=(CertContext &&mv)
            {
                std::swap(handle, mv.handle);
                return *this;
            }
            ~CertContext() { if (*this) CertFreeCertificateContext(handle); }
            explicit operator bool()const { return handle != NULL; }
        };
        struct CertStore
        {
            HCERTSTORE handle;
            explicit CertStore(HCERTSTORE handle) : handle(handle) {}
            ~CertStore() { if (*this) CertCloseStore(handle, 0); }
            explicit operator bool()const { return handle != NULL; }
        };
        #pragma warning(push)
        #pragma warning( disable : 4706 ) //Doesnt like a while(item = next()) loop
        CertContext get_client_cert_by_host(const std::string &hostname)
        {
            CertStore store(CertOpenSystemStoreW(NULL, L"My"));
            if (!store) throw std::runtime_error("Failed to open user certificate store");

            char *usage_id = "szOID_PKIX_KP_SERVER_AUTH";
            CERT_ENHKEY_USAGE usage;
            usage.cUsageIdentifier = 1; //number of elements
            usage.rgpszUsageIdentifier = &usage_id;

            CertContext ctx;
            while (ctx.handle = CertFindCertificateInStore(store.handle, X509_ASN_ENCODING,
                CERT_FIND_OPTIONAL_ENHKEY_USAGE_FLAG, CERT_FIND_ENHKEY_USAGE, &usage,
                ctx.handle))
            {
                static const DWORD MAX_LEN = 256;
                char name_buffer[MAX_LEN];
                auto ret = CertGetNameStringA(ctx.handle, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, name_buffer, MAX_LEN);
                if (ret)
                {
                    if (ret - 1 == hostname.size() && memcmp(name_buffer, hostname.data(), ret - 1) == 0)
                    {
                        return ctx;
                    }
                }
            }
            throw std::runtime_error("Failed to find server certificate");
        }
        #pragma warning(pop)
    }*/

    SchannelServerSocket::SchannelServerSocket(TcpSocket &&socket, const PrivateCert &cert)
        : SchannelSocket(std::move(socket))
    {
        server = true;
        tls_accept(cert);
    }

    void SchannelServerSocket::tls_accept(const PrivateCert &cert)
    {
        create_credentials(cert);
        server_handshake_loop();
        alloc_buffers();
    }
    void SchannelServerSocket::create_credentials(const PrivateCert &cert)
    {
        // Get server certificate
        // TODO: Accept hostname, store, etc., as config
        auto cert_ctx = (PCCERT_CONTEXT)cert.get();
        TimeStamp expiry;
        SCHANNEL_CRED cred = {0};
        cred.dwVersion = SCHANNEL_CRED_VERSION;
        cred.cCreds = 1;
        cred.paCred = &cert_ctx;
        cred.grbitEnabledProtocols = SP_PROT_TLS1_2_SERVER;
        cred.dwFlags = SCH_USE_STRONG_CRYPTO;

        auto status = sspi->AcquireCredentialsHandle(
            nullptr, UNISP_NAME, SECPKG_CRED_INBOUND, nullptr, &cred, nullptr, nullptr,
            &credentials.handle, &expiry);
        if (status != SEC_E_OK) throw std::runtime_error("Failed to convert PCCERT_CONTEXT to CredHandle");
    }
    void SchannelServerSocket::server_handshake_loop()
    {
        DWORD sspi_flags =
            ASC_REQ_SEQUENCE_DETECT |
            ASC_REQ_REPLAY_DETECT |
            ASC_REQ_CONFIDENTIALITY |
            ASC_REQ_EXTENDED_ERROR |
            ASC_REQ_ALLOCATE_MEMORY |
            ASC_REQ_STREAM;
        //TODO: ASC_REQ_MUTUAL_AUTH to request a client certificate

        TimeStamp expiry;
        SECURITY_STATUS status;
        SecBufferSingleAutoFree out_buffer;
        SecBuffer in_buffers[2];
        SecBufferDesc in_buffer_desc = {SECBUFFER_VERSION, 2, in_buffers};

        status = SEC_E_INCOMPLETE_MESSAGE;
        bool do_read = true;
        while (true)
        {
            // Read data if needed
            if (recv_encrypted_buffer.empty() || status == SEC_E_INCOMPLETE_MESSAGE)
            {
                if (do_read)
                {
                    if (!recv_encrypted())
                        throw std::runtime_error("Socket closed before TLS handshake complete");
                }
                else do_read = true;
            }

            // Set up buffers
            in_buffers[0] = {(DWORD)recv_encrypted_buffer.size(), SECBUFFER_TOKEN, recv_encrypted_buffer.data()};
            in_buffers[1] = {0, SECBUFFER_EMPTY, nullptr};
            out_buffer.buffer = {0, SECBUFFER_TOKEN, nullptr};

            // Call SSPI
            DWORD sspi_out_flags;
            status = sspi->AcceptSecurityContext(
                &credentials.handle,
                context ? &context.handle : nullptr, // Context after first call
                &in_buffer_desc, sspi_flags, 0,
                context ? nullptr : &context.handle, // Create new context on first call
                &out_buffer.desc, &sspi_out_flags, &expiry);

            // Send requested output
            if (status == SEC_E_OK || status == SEC_I_CONTINUE_NEEDED ||
                (FAILED(status) && (0 != (sspi_out_flags & ASC_RET_EXTENDED_ERROR))))
            {
                if (out_buffer.buffer.cbBuffer && out_buffer.buffer.pvBuffer)
                {
                    send_sec_buffers(out_buffer.desc);
                    out_buffer.free();
                }
            }
            if (status == SEC_E_INCOMPLETE_MESSAGE)
            {
                // Read more data and retry
                continue;
            }
            else if (status == SEC_E_OK)
            {
                // Completed
                // TODO: Check client certificate here
                // Might have read too much data before, so store for later
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
            else if (FAILED(status))
            {
                // Fatal error
                throw NetworkError("TLS server acccept handshake failed with " + std::to_string(status));
            }
            else
            {
                // Not complete. Move any spare input to the start of the buffer then loop.
                assert(recv_encrypted_buffer.size() >= in_buffers[1].cbBuffer);
                recv_encrypted_buffer.erase(
                    recv_encrypted_buffer.begin(),
                    recv_encrypted_buffer.begin() + recv_encrypted_buffer.size() - in_buffers[1].cbBuffer);
            }
        }
    }

    void SchannelServerSocket::async_create(AsyncIo &aio, TcpSocket &&socket, const PrivateCert &cert,
        std::function<void()> complete, AsyncIo::ErrorHandler error)
    {
        create_credentials(cert);
        tcp = std::move(socket);
        async_server_handshake_recv(aio, complete, error);
    }
    void SchannelServerSocket::async_server_handshake_recv(AsyncIo &aio, std::function<void()> complete, AsyncIo::ErrorHandler error)
    {
        auto p = recv_encrypted_buffer.size();
        recv_encrypted_buffer.resize(p + 4096);
        tcp.async_recv(aio, recv_encrypted_buffer.data() + p, 4096,
            [this, &aio, p, complete, error](size_t len)
            {
                recv_encrypted_buffer.resize(p + len);
                async_server_handshake_next(aio, complete, error);
            },
            error);
    }
    void SchannelServerSocket::async_server_handshake_next(AsyncIo &aio, std::function<void()> complete, AsyncIo::ErrorHandler error)
    {
        try
        {
            while (true)
            {
                SecBuffer in_buffers[2] = {
                    { (DWORD)recv_encrypted_buffer.size(), SECBUFFER_TOKEN, recv_encrypted_buffer.data() },
                    { 0, SECBUFFER_EMPTY, nullptr }
                };
                SecBufferDesc in_buffer_desc = { SECBUFFER_VERSION, 2, in_buffers };

                SecBufferSingleAutoFree out_buffer;
                out_buffer.buffer = { 0, SECBUFFER_TOKEN, nullptr };

                // Call SSPI
                TimeStamp expiry;
                DWORD sspi_flags =
                    ASC_REQ_SEQUENCE_DETECT |
                    ASC_REQ_REPLAY_DETECT |
                    ASC_REQ_CONFIDENTIALITY |
                    ASC_REQ_EXTENDED_ERROR |
                    ASC_REQ_ALLOCATE_MEMORY |
                    ASC_REQ_STREAM;
                DWORD sspi_out_flags;
                auto status = sspi->AcceptSecurityContext(
                    &credentials.handle,
                    context ? &context.handle : nullptr, // Context after first call
                    &in_buffer_desc, sspi_flags, 0,
                    context ? nullptr : &context.handle, // Create new context on first call
                    &out_buffer.desc, &sspi_out_flags, &expiry);

                // Send requested output
                if (status == SEC_E_OK || status == SEC_I_CONTINUE_NEEDED ||
                    (FAILED(status) && (0 != (sspi_out_flags & ASC_RET_EXTENDED_ERROR))))
                {
                    if (out_buffer.buffer.cbBuffer && out_buffer.buffer.pvBuffer)
                    {
                        send_sec_buffers(out_buffer.desc); //TODO: Make async
                        out_buffer.free();
                    }
                }
                if (status == SEC_E_INCOMPLETE_MESSAGE)
                {
                    // Read more data and retry
                    return async_server_handshake_recv(aio, complete, error);
                }
                else if (status == SEC_E_OK)
                {
                    // Completed
                    // TODO: Check client certificate here
                    // Might have read too much data before, so store for later
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
                    alloc_buffers();
                    return complete();
                }
                else if (FAILED(status))
                {
                    // Fatal error
                    throw NetworkError("TLS server acccept handshake failed with " + std::to_string(status));
                }
                else
                {
                    // Not complete. Move any spare input to the start of the buffer then loop.
                    assert(recv_encrypted_buffer.size() >= in_buffers[1].cbBuffer);
                    recv_encrypted_buffer.erase(
                        recv_encrypted_buffer.begin(),
                        recv_encrypted_buffer.begin() + recv_encrypted_buffer.size() - in_buffers[1].cbBuffer);
                }
            }
        }
        catch (const std::exception &)
        {
            error();
        }
    }
}
