#include <cstdlib>
#include <string>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options.hpp>

#include <appbase/application.hpp>

#include <koinos/exception.hpp>
#include <koinos/log.hpp>

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
  _  __     _
 | |/ /___ (_)_ __   ___  ___
 | ' // _ \| | '_ \ / _ \/ __|
 | . \ (_) | | | | | (_) \__ \
 |_|\_\___/|_|_| |_|\___/|___/)BANNER";

   std::cout << BANNER << std::endl;
#ifdef IS_TEST_NET
   std::cout << "       ...launching test network" << std::endl;
#else
   std::cout << "       ...launching main network" << std::endl;
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

      koinos::initialize_logging( appbase::app().data_dir(), "koinosd_%3N.log" );
      appbase::app().set_writer( []( const std::string& msg )
      {
         LOG(info) << msg;
      });
      appbase::app().startup();
      LOG(info) << "koinosd startup complete";
      appbase::app().exec();
      LOG(info) << "exited cleanly";

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
