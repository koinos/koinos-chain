#include <cstdlib>
#include <string>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options.hpp>

#include <appbase/application.hpp>

#include <koinos/exception.hpp>
#include <koinos/log/log.hpp>

#include <koinos/manifest/plugins.hpp>

#include <koinos/plugins/chain/chain_plugin.hpp>
#include <koinos/plugins/block_producer/block_producer_plugin.hpp>

const std::string& version_string()
{
   static std::string v_str = "0.1";
   return v_str;
}

void splash()
{
const char* BANNER = R"BANNER(
   ▄█   ▄█▄  ▄██████▄   ▄█  ███▄▄▄▄    ▄██████▄     ▄████████
  ███ ▄███▀ ███    ███ ███  ███▀▀▀██▄ ███    ███   ███    ███
  ███▐██▀   ███    ███ ███▌ ███   ███ ███    ███   ███    █▀
 ▄█████▀    ███    ███ ███▌ ███   ███ ███    ███   ███
▀▀█████▄    ███    ███ ███▌ ███   ███ ███    ███ ▀███████████
  ███▐██▄   ███    ███ ███  ███   ███ ███    ███          ███
  ███ ▀███▄ ███    ███ ███  ███   ███ ███    ███    ▄█    ███
  ███   ▀█▀  ▀██████▀  █▀    ▀█   █▀   ▀██████▀   ▄████████▀
  ▀)BANNER";
const char* LINE_BREAK = R"LINE_BREAK(
-------------------------------------------------------------)LINE_BREAK";
const char* TESTNET = R"TESTNET(
                                    ...launching test network)TESTNET";
const char* MAINNET = R"MAINNET(
                                    ...launching main network)MAINNET";
   std::cout << BANNER;
#ifdef IS_TEST_NET
   std::cout << TESTNET;
#else
   std::cout << MAINNET;
#endif
   std::cout << std::endl;
}

int main( int argc, char** argv )
{
   try
   {
      splash();

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
      koinos::log::initialize( appbase::app().data_dir(), "koinosd_%3N.log" );
      appbase::app().exec();
      LOG(info) << "exited cleanly";

      return EXIT_SUCCESS;
   }
   catch ( const koinos::exception::koinos_exception& e )
   {
      LOG(fatal) << e.to_string() << std::endl;
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
