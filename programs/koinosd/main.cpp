#include <cstdlib>
#include <string>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options.hpp>

#include <appbase/application.hpp>
#include <koinos/exception.hpp>

#include <koinos/manifest/plugins.hpp>

#include <koinos/plugins/chain/chain_plugin.hpp>
#include <koinos/plugins/block_producer/block_producer_plugin.hpp>

const std::string& version_string()
{
   static std::string v_str = "0.1";
   return v_str;
}

void info()
{
#ifdef IS_TEST_NET
      std::cout << "------------------------------------------------------\n\n";
      std::cout << "            starting test network\n\n";
      std::cout << "------------------------------------------------------\n";
//      auto initminer_private_key = koinos::utilities::key_to_wif( KOINOS_INIT_PRIVATE_KEY );
//      std::cout << "initminer public key: " << KOINOS_INIT_PUBLIC_KEY_STR << "\n";
//      std::cout << "initminer private key: " << initminer_private_key << "\n";
//      std::cout << "blockchain version: " << std::string( KOINOS_BLOCKCHAIN_VERSION ) << "\n";
      std::cout << "------------------------------------------------------\n";
#else
      std::cout << "------------------------------------------------------\n\n";
      std::cout << "            starting koinos network\n\n";
      std::cout << "------------------------------------------------------\n";
//      std::cout << "initminer public key: " << KOINOS_INIT_PUBLIC_KEY_STR << "\n";
//      std::cout << "chain id: " << std::string( KOINOS_CHAIN_ID ) << "\n";
//      std::cout << "blockchain version: " << std::string( KOINOS_BLOCKCHAIN_VERSION ) << "\n";
      std::cout << "------------------------------------------------------\n";
#endif
}

int main( int argc, char** argv )
{
   try
   {
      boost::program_options::options_description options;
      appbase::app().add_program_options( boost::program_options::options_description(), options );

      koinos::plugins::register_plugins();

      appbase::app().set_version_string( version_string() );
      appbase::app().set_app_name( "koinosd" );

      appbase::app().set_default_plugins<
         koinos::plugins::chain::chain_plugin,
         koinos::plugins::block_producer::block_producer_plugin >();

      bool initialized = appbase::app().initialize<
         koinos::plugins::chain::chain_plugin >
         ( argc, argv );

      if( !initialized ) return EXIT_SUCCESS;

      appbase::app().startup();
      appbase::app().exec();
      std::cout << "exited cleanly\n";

      return EXIT_SUCCESS;
   }
   catch ( const koinos::exception::koinos_exception& e )
   {
      std::cerr << e.to_string() << std::endl;
   }
   catch ( const boost::exception& e )
   {
      std::cerr << boost::diagnostic_information( e ) << std::endl;
   }
   catch ( const std::exception& e )
   {
      std::cerr << e.what() << std::endl;
   }
   catch ( ... )
   {
      std::cerr << "unknown exception" << std::endl;
   }

   return EXIT_FAILURE;
}
