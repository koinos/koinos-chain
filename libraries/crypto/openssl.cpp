#include <koinos/crypto/openssl.hpp>

#include <cstdlib>
#include <string>
#include <stdlib.h>

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
// TODO: Replace OPENSSL_config()

namespace koinos::crypto
{
   struct openssl_scope
   {
      static boost::filesystem::path _configurationFilePath;
      openssl_scope()
      {
         ERR_load_crypto_strings();
         OpenSSL_add_all_algorithms();

         const boost::filesystem::path& boostPath = _configurationFilePath;
         if(boostPath.empty() == false)
         {
         std::string varSetting("OPENSSL_CONF=");
         varSetting += _configurationFilePath.generic_string();
#if defined(WIN32)
         _putenv((char*)varSetting.c_str());
#else
         putenv((char*)varSetting.c_str());
#endif
         }

         OPENSSL_config(nullptr);
      }

      ~openssl_scope()
      {
         EVP_cleanup();
         ERR_free_strings();
      }
   };

   void store_configuration_path(const boost::filesystem::path& filePath)
   {
      openssl_scope::_configurationFilePath = filePath;
   }

   int init_openssl()
   {
      static openssl_scope ossl;
      return 0;
   }

} // koinos::crypto
