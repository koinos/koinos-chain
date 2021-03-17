#include <iostream>

#include <boost/program_options.hpp>

#include <koinos/exception.hpp>
#include <koinos/log.hpp>
#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/pack/classes.hpp>

void sign_transaction( koinos::protocol::transaction& transaction, koinos::crypto::private_key& transaction_signing_key )
{
   // Signature is on the hash of the active data
   koinos::multihash digest = koinos::crypto::hash( CRYPTO_SHA2_256_ID, transaction.active_data );
   auto signature = transaction_signing_key.sign_compact( digest );
   koinos::pack::to_variable_blob( transaction.signature_data, signature );
}

int main( int argc, char** argv )
{
   try
   {
      boost::program_options::options_description options;

      std::cout << std::cin.rdbuf();
      
      return EXIT_SUCCESS;
   }
   catch ( const boost::exception& e )
   {
      LOG(fatal) << boost::diagnostic_information( e ) << std::endl;
   }
   catch ( const std::exception& e )
   {
      LOG(fatal) << e.what() << std::endl;
   }
   catch ( ... )
   {
      LOG(fatal) << "unknown exception" << std::endl;
   }

   return EXIT_FAILURE;
}
