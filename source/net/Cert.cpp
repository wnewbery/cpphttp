#define _CRT_SECURE_NO_DEPRECATE
#include "net/Cert.hpp"
#include <cassert>
#if defined(_WIN32) && !defined(HTTP_USE_OPENSSL)
#   include <Windows.h>
#   include <memory>
#   include <vector>
#else
#   include "net/OpenSslCert.hpp"
#   include <openssl/pem.h>
#   include <openssl/err.h>
#   include <openssl/pkcs12.h>
#   include <cstdio>
#endif

namespace http
{
    PrivateCert::PrivateCert(PrivateCert &&mv) : native(mv.native) { mv.native = nullptr; }
    PrivateCert::~PrivateCert() { if (native) free(native); }
#if defined(_WIN32) && !defined(HTTP_USE_OPENSSL)
    namespace
    {
        /**LocalAlloc / LocalFree memory.*/
        template<class T>
        struct LocalBuf
        {
            T *p = nullptr;
            ~LocalBuf() { if (p) LocalFree(p); }
        };

        std::string cert_file_str(const std::string &file_name)
        {
            //Provides RAII for the file HANdLE
            struct File
            {
                File(const std::string &file_name)
                {
                    handle = CreateFileA(file_name.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (handle == INVALID_HANDLE_VALUE)
                        throw std::runtime_error("Failed to open certificate file " + file_name);
                }
                ~File()
                {
                    CloseHandle(handle);
                }
                HANDLE handle;
            };
            File file(file_name);

            LARGE_INTEGER size;
            if (!GetFileSizeEx(file.handle, &size) || size.HighPart > 0 || size.LowPart > 1024 * 1024 * 10)
                throw std::runtime_error("Certificate file too large" + file_name);

            std::string out;
            out.resize(size.LowPart);

            DWORD len;
            if (!ReadFile(file.handle, &out[0], size.LowPart, &len, NULL) || len != size.LowPart)
                throw std::runtime_error("Failed to read certificate file " + file_name);

            return out;
        }
    }

    void PrivateCert::free(const void *native)
    {
        CertFreeCertificateContext((PCCERT_CONTEXT)native);
    }
    const void* PrivateCert::dup(const void *native)
    {
        return CertDuplicateCertificateContext((PCCERT_CONTEXT)native);
    }

    PrivateCert load_pfx_cert(const std::string &file, const std::string &password)
    {
        struct CertStore
        {
            HCERTSTORE h;
            CertStore(HCERTSTORE h) : h(h) {}
            ~CertStore() { if (h) { CertCloseStore(h, 0); } }
        };
        // Need the password in UTF-16
        std::unique_ptr<wchar_t[]> password_w(new wchar_t[password.size() + 1]);
        auto password_len = (DWORD)MultiByteToWideChar(CP_UTF8, 0, password.data(), (int)password.size(), password_w.get(), (int)password.size());
        if (!password_len) throw std::runtime_error("MultiByteToWideChar failed");
        password_w[password_len] = 0;
        // Load and decrypt the certificate and key
        auto pfx_bin = cert_file_str(file);
        CRYPT_DATA_BLOB pfx = {0};
        pfx.cbData = (DWORD)pfx_bin.size();
        pfx.pbData = (BYTE*)&pfx_bin[0];
        CertStore store(PFXImportCertStore(&pfx, password_w.get(), 0));
        if (!store.h) throw std::runtime_error("Importing " + file + " failed");
        // Get the certificate with linked key
        PrivateCert cert(CertEnumCertificatesInStore(store.h, nullptr));
        if (!cert) throw std::runtime_error("Importing " + file + " failed");

        return cert;
    }

    PrivateCert load_pem_priv_cert(const std::string &crt_file, const std::string &key_file)
    {
        struct CryptProv
        {
            HCRYPTPROV h = NULL;
            ~CryptProv() { if (h) CryptReleaseContext(h, 0); }
        };
        auto pem_file_to_bin = [](const std::string &file_name)
        {
            auto pem = cert_file_str(file_name);

            std::vector<unsigned char> bytes;
            bytes.resize(pem.size());
            auto bytes_len = (DWORD)pem.size();

            if (!CryptStringToBinaryA(
                pem.c_str(), (DWORD)pem.size(),
                CRYPT_STRING_BASE64HEADER,
                &bytes[0], &bytes_len, nullptr, nullptr))
                throw std::runtime_error("Converting " + file_name + " as PEM to binary failed");

            // Truncate since base64 decodeing compresses
            assert(bytes_len <= bytes.size());
            bytes.resize(bytes_len);

            return bytes;
        };

        auto crt_bin = pem_file_to_bin(crt_file);
        auto key_bin = pem_file_to_bin(key_file);


        PrivateCert cert(CertCreateCertificateContext(X509_ASN_ENCODING, crt_bin.data(), (DWORD)crt_bin.size()));
        if (!cert) throw std::runtime_error("Decoding " + crt_file + " failed");

        // Add the key to the certificate. Windows stores the key seperate in the API's to make it
        // difficult to get the private key from an existing context.

        // TODO: Not complete. CryptDecodeObjectEx does not seem to support keys generated by OpenSSL
        auto cert_ctx = (PCCERT_CONTEXT)cert.get();
        LocalBuf<BYTE> key_decoded;
        DWORD key_decoded_len = 0;
        if (!CryptDecodeObjectEx(PKCS_7_ASN_ENCODING | X509_ASN_ENCODING, PKCS_RSA_PRIVATE_KEY,
            key_bin.data(), (DWORD)key_bin.size(), CRYPT_DECODE_ALLOC_FLAG,
            nullptr, &key_decoded.p, &key_decoded_len))
            throw std::runtime_error("Failed to decode " + key_file);

        CRYPT_KEY_PROV_INFO prov_info = {0};
        auto prov_info_len = (DWORD)sizeof(prov_info);
        if (!CertGetCertificateContextProperty(cert_ctx, CERT_KEY_PROV_INFO_PROP_ID, &prov_info, &prov_info_len))
            throw std::runtime_error("CertGetCertificateContextProperty failed");

        CryptProv crypt_prov;
        if (!CryptAcquireContext(&crypt_prov.h, prov_info.pwszContainerName, prov_info.pwszProvName, prov_info.dwProvType, CRYPT_SILENT))
            throw std::runtime_error("CryptAcquireContext failed");

        HCRYPTKEY key;
        if (!CryptImportKey(crypt_prov.h, key_decoded.p, key_decoded_len, NULL, 0, &key))
            throw std::runtime_error("CryptImportKey failed");

        //TODO: See if CryptImportKey links the key to the certificate or not

        return cert;
    }
#else
    struct File
    {
        FILE *f;
        File(const std::string &file_name)
          : f(fopen(file_name.c_str(), "rb"))
        {
            if (!f) throw std::runtime_error("Failed to open certificate file " + file_name);
        }
        ~File()
        {
            if (f) fclose(f);
        }
    };
    struct OpenSslDeleter
    {
        void operator()(PKCS12 *pfx)const { PKCS12_free(pfx); }
    };

    OpenSslPrivateCertData::~OpenSslPrivateCertData()
    {
        EVP_PKEY_free(pkey);
        X509_free(cert);
        sk_X509_pop_free(ca, X509_free);
    }

    void PrivateCert::free(std::shared_ptr<OpenSslPrivateCertData> native)
    {
    }
    std::shared_ptr<OpenSslPrivateCertData> PrivateCert::dup(std::shared_ptr<OpenSslPrivateCertData> native)
    {
        return native;
    }

    PrivateCert load_pfx_cert(const std::string &file_name, const std::string &password)
    {
        File f(file_name);

        std::unique_ptr<PKCS12,OpenSslDeleter> pfx(d2i_PKCS12_fp(f.f, nullptr));
        if (!pfx) throw std::runtime_error("Failed to read pfx certificate " + file_name);

        auto data = std::make_shared<OpenSslPrivateCertData>();
        if (!PKCS12_parse(pfx.get(), password.c_str(), &data->pkey, &data->cert, &data->ca))
            throw std::runtime_error("Failed to parse pfx certificate " + file_name);

        PrivateCert out(data); // For RAII

        if (!data->pkey || !data->cert)
            throw std::runtime_error(file_name + " did not contain key and certificate");

        return out;
    }

    PrivateCert load_pem_priv_cert(const std::string &, const std::string &)
    {
        std::terminate();
    }
#endif
}
