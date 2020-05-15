#pragma once
#include <boost/filesystem/path.hpp>

#include <openssl/ec.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/ecdsa.h>
#include <openssl/ecdh.h>
#include <openssl/sha.h>
#include <openssl/obj_mac.h>

namespace koinos::crypto
{
    /** Allows to explicitly specify OpenSSL configuration file path to be loaded at OpenSSL library init.
        If not set OpenSSL will try to load the conf. file (openssl.cnf) from the path it was
        configured with what caused serious Keyhotee startup bugs on some Win7, Win8 machines.
        \warning to be effective this method should be used before any part using OpenSSL, especially
        before init_openssl call
    */
    void store_configuration_path(const boost::filesystem::path& filePath);
    int init_openssl();

} //  koinos::crypte
