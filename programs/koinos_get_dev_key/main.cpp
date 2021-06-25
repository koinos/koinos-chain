#include <iostream>
#include <fstream>

#include <boost/program_options.hpp>

#include <koinos/exception.hpp>
#include <koinos/log.hpp>
#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/pack/classes.hpp>
#include <koinos/util.hpp>

// Command line option definitions
#define HELP_OPTION        "help"
#define HELP_FLAG          "h"

#define NUM_KEYS_OPTION    "num"
#define NUM_KEYS_FLAG      "n"
const uint64_t NUM_KEYS_DEFAULT = 1;

#define SEED_OPTION        "seed"
#define SEED_FLAG          "s"

#define OUTPUT_FILE_OPTION "out"
#define OUTPUT_FILE_FLAG   "o"
const std::string OUTPUT_FILE_DEFAULT = "private.key";



int main( int argc, char** argv )
{
   try
   {
      // Setup command line options
      boost::program_options::options_description options( "Options" );
      options.add_options()
      ( HELP_OPTION        "," HELP_FLAG,        "print usage message" )
      ( SEED_OPTION        "," SEED_FLAG,        boost::program_options::value< std::string >()->default_value( "" ), "Seed to generate a key with" )
      ( NUM_KEYS_OPTION    "," NUM_KEYS_FLAG,    boost::program_options::value< uint64_t >()->default_value( NUM_KEYS_DEFAULT ), "number of keys to generate" )
      ( OUTPUT_FILE_OPTION "," OUTPUT_FILE_FLAG, boost::program_options::value< std::string >()->default_value( OUTPUT_FILE_DEFAULT ), "file to output keys to" );
      ;

      // Parse command-line options
      boost::program_options::variables_map args;
      boost::program_options::store( boost::program_options::parse_command_line( argc, argv, options ), args );

      // Handle help message
      if ( args.count( HELP_OPTION ) )
      {
         std::cout << options << std::endl;
         return EXIT_SUCCESS;
      }

      auto seed = args[ SEED_OPTION ].as< std::string >();
      if ( !seed.size() )
      {
         seed = koinos::random_alphanumeric( 64 );
      }

      auto output_file = std::filesystem::path( args[ OUTPUT_FILE_OPTION ].as< std::string >() );
      if ( output_file.is_relative() )
      {
         output_file = std::filesystem::current_path() / output_file;
      }

      auto num_keys = args[ NUM_KEYS_OPTION ].as< uint64_t >();

      std::cout << "koinos_get_dev_key generates development keys.\n\n";
      std::cout << "WARNING!!!\n\n";
      std::cout << "- Keys are not generated or stored in a secure manner.\n";
      std::cout << "- Key generation may not be consistent across versions of koinos_get_dev_key.\n\n";
      std::cout << "For these reasons, keys generated with koinos_get_dev_key should ONLY be used for development purposes.\n\n";

      std::ofstream outstream;
      outstream.open( output_file.string() );

      for ( uint64_t i = 0; i < num_keys; i++ )
      {
         auto secret = koinos::crypto::hash_n( CRYPTO_SHA2_256_ID, seed, i );
         auto private_key = koinos::crypto::private_key::regenerate( secret );
         std::cout << "Generated key: " << private_key.get_public_key().to_address() << std::endl;
         outstream << private_key.to_wif() << std::endl;
      }

      outstream.close();

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
