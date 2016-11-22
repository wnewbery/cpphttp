#pragma once
#include <openssl/evp.h>
#include <openssl/safestack.h>
#include <openssl/x509.h>
namespace http
{
    struct OpenSslPrivateCertData
    {
        EVP_PKEY *pkey;
        X509 *cert;
        STACK_OF(X509) *ca;
        
        ~OpenSslPrivateCertData();
    };
}
