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
      static std::filesystem::path _config_filepath;
      openssl_scope()
      {
         ERR_load_crypto_strings();
         OpenSSL_add_all_algorithms();

         const std::filesystem::path& path = _config_filepath;
         if( path.empty() == false )
         {
         std::string varSetting("OPENSSL_CONF=");
         varSetting += _config_filepath.generic_string();
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

   std::filesystem::path openssl_scope::_config_filepath;

   void store_configuration_path( const std::filesystem::path& file_path )
   {
      openssl_scope::_config_filepath = file_path;
   }

   int init_openssl()
   {
      static openssl_scope ossl;
      return 0;
   }

} // koinos::crypto
