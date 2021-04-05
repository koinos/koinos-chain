#include <iostream>
#include <fstream>

#include <boost/program_options.hpp>

#include <koinos/exception.hpp>
#include <koinos/log.hpp>
#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/pack/classes.hpp>

// Command line option definitions
#define HELP_OPTION        "help"
#define HELP_FLAG          "h"

#define PRIVATE_KEY_OPTION "private-key"
#define PRIVATE_KEY_FLAG   "p"

#define WRAP_OPTION        "wrap"
#define WRAP_FLAG          "w"

// Sign the given transaction
void sign_transaction( koinos::protocol::transaction& transaction, koinos::crypto::private_key& transaction_signing_key )
{
   // Signature is on the hash of the active data
   koinos::multihash digest = koinos::crypto::hash( CRYPTO_SHA2_256_ID, transaction.active_data );
   auto signature = transaction_signing_key.sign_compact( digest );
   koinos::pack::to_variable_blob( transaction.signature_data, signature );
}

// Wrap the given transaction in a request
koinos::rpc::chain::chain_rpc_request wrap_transaction( koinos::protocol::transaction& transaction )
{
   // Construct a submit_transaction_request from the transaction
   koinos::rpc::chain::submit_transaction_request transaction_request;
   transaction_request.transaction = transaction;
   transaction_request.verify_passive_data = true;
   transaction_request.verify_transaction_signatures = true;

   // Put the request in a chain_request
   koinos::rpc::chain::chain_rpc_request chain_request;
   chain_request = transaction_request;

   return chain_request;
}

// Read a base58 WIF private key from the given file
koinos::crypto::private_key read_keyfile( std::string key_filename )
{
   // Read base58 wif string from given file
   std::string key_string;
   std::ifstream instream;
   instream.open( key_filename );
   std::getline( instream, key_string );
   instream.close();

   // Create and return the key from the wif
   auto key = koinos::crypto::private_key::from_wif( key_string );
   return key;
}

int main( int argc, char** argv )
{
   try
   {
      // Setup command line options
      boost::program_options::options_description options( "Options" );
      options.add_options()
      ( HELP_OPTION "," HELP_FLAG,               "print usage message" )
      ( PRIVATE_KEY_OPTION "," PRIVATE_KEY_FLAG, boost::program_options::value< std::string >()->default_value( "private.key" ), "private key file" )
      ( WRAP_OPTION "," WRAP_FLAG,               "wrap signed transaction in a request" )
      ;

      // Parse command-line options
      boost::program_options::variables_map vm;
      boost::program_options::store( boost::program_options::parse_command_line( argc, argv, options ), vm );

      // Handle help message
      if ( vm.count( HELP_OPTION ) )
      {
         std::cout << "Koinos Transaction Signing Tool" << std::endl;
         std::cout << "Accepts a json transaction to sign via STDIN" << std::endl;
         std::cout << "Returns the signed transaction via STDOUT" << std::endl << std::endl;
         std::cout << options << std::endl;
         return EXIT_SUCCESS;
      }

      // Read options into variables
      std::string key_filename = vm[ PRIVATE_KEY_OPTION ].as< std::string >();
      bool wrap                = vm.count( WRAP_OPTION );
      
      // Read the keyfile
      auto private_key = read_keyfile( key_filename );

      // Read STDIN to a string
      std::string transaction_json;
      std::getline( std::cin, transaction_json );

      // Parse and deserialize the json to a transaction
      auto j = koinos::pack::json::parse( transaction_json );
      koinos::protocol::transaction transaction;
      koinos::pack::from_json( j, transaction );

      // Sign the transaction
      sign_transaction( transaction, private_key );

      // Set the transaction id
      transaction.id = koinos::crypto::hash( CRYPTO_SHA2_256_ID, transaction.active_data );

      if (wrap) // Wrap the transaction if requested
      {
         auto request = wrap_transaction( transaction );
         std::cout << request << std::endl;
      }
      else // Else simply output the signed transaction
      {
         std::cout << transaction << std::endl;
      }

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
