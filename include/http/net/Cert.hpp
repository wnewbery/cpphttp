#pragma once
#include <algorithm>
#include <string>
namespace http
{

    /**A single private key and certificate used to authenticate a TLS server or client.*/
    class PrivateCert
    {
    public:
        PrivateCert() : native(nullptr) {}
        explicit PrivateCert(const void *native) : native(native) { }
        PrivateCert(const PrivateCert &cp) : native(dup(cp.native)) { }
        PrivateCert(PrivateCert &&mv) : native(mv.native) { mv.native = nullptr; }
        ~PrivateCert() { if (native) free(native); }

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
        const void *get()const { return native; }
    private:
        const void *native;

        static void free(const void *native);
        static const void* dup(const void *native);
    };

    /**Load a certificate with private key from a PKCS#12 archive.*/
    PrivateCert load_pfx_cert(const std::string &file, const std::string &password);

    /**Load a certificate with private key from a pair of pem files.
     * NOT COMPLETE
     */
    PrivateCert load_pem_priv_cert(const std::string &crt_file, const std::string &key_file);
}
