#pragma once
#include <algorithm>
#include <memory>
#include <string>
namespace http
{
    struct OpenSslPrivateCertData;
    /**A single private key and certificate used to authenticate a TLS server or client.*/
    class PrivateCert
    {
    public:
    #if defined(_WIN32) && !defined(HTTP_USE_OPENSSL)
        /**PCCERT_CONTEXT*/
        typedef const void *Native;
    #else
        typedef std::shared_ptr<OpenSslPrivateCertData> Native;
    #endif
        PrivateCert() : native(nullptr) {}
        explicit PrivateCert(Native native) : native(native) { }
        PrivateCert(const PrivateCert &cp) : native(dup(cp.native)) { }
        PrivateCert(PrivateCert &&mv);
        ~PrivateCert();

        PrivateCert& operator = (const PrivateCert &cp)
        {
            if (native) free(native);
            if (cp.native) native = dup(cp.native);
            return *this;
        }
        PrivateCert& operator = (PrivateCert &&mv)
        {
            std::swap(native, mv.native);
            return *this;
        }

        explicit operator bool()const
        {
            return native != nullptr;
        }
        Native get()const { return native; }
    private:
        Native native;

        static void free(Native native);
        static Native dup(Native native);
    };

    /**Load a certificate with private key from a PKCS#12 archive.*/
    PrivateCert load_pfx_cert(const std::string &file, const std::string &password);

    /**Load a certificate with private key from a pair of pem files.
     * NOT COMPLETE
     */
    PrivateCert load_pem_priv_cert(const std::string &crt_file, const std::string &key_file);
}
