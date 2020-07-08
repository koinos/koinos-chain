#include <koinos/plugins/chain/chain_plugin.hpp>

#include <koinos/log.hpp>
#include <koinos/util.hpp>

#include <mira/database_configuration.hpp>

#include <filesystem>
#include <fstream>

namespace koinos::plugins::chain {

using namespace appbase;

namespace detail {

class chain_plugin_impl
{
   public:
      chain_plugin_impl()  {}
      ~chain_plugin_impl() {}

      void write_default_database_config( std::filesystem::path &p );

      std::filesystem::path     state_dir;
      std::filesystem::path     database_cfg;

      koinos::chain::controller controller;
};

void chain_plugin_impl::write_default_database_config( std::filesystem::path &p )
{
   LOG(info) << "writing database configuration: " << p.string();
   std::ofstream config_file( p, std::ios::binary );
   config_file << mira::utilities::default_database_configuration();
}

} // detail


chain_plugin::chain_plugin() : my( new detail::chain_plugin_impl() ) {}
chain_plugin::~chain_plugin(){}

koinos::chain::controller& chain_plugin::controller() { return my->controller; }
const koinos::chain::controller& chain_plugin::controller() const { return my->controller; }

std::filesystem::path chain_plugin::state_dir() const
{
   return my->state_dir;
}

void chain_plugin::set_program_options( options_description& cli, options_description& cfg )
{
   cfg.add_options()
         ("state-dir", bpo::value< std::filesystem::path >()->default_value("blockchain"),
            "the location of the blockchain state files (absolute path or relative to application data dir)")
         ("database-config", bpo::value< std::filesystem::path >()->default_value("database.cfg"), "The database configuration file location")
         ;
   cli.add_options()
         ("force-open", bpo::bool_switch()->default_value(false), "force open the database, skipping the environment check")
         ;
}

void chain_plugin::plugin_initialize( const variables_map& options )
{
   my->state_dir = app().data_dir() / "blockchain";

   if( options.count("state-dir") )
   {
      auto sfd = options.at("state-dir").as<std::filesystem::path>();
      if(sfd.is_relative())
         my->state_dir = app().data_dir() / sfd;
      else
         my->state_dir = sfd;
   }

   my->database_cfg = options.at( "database-config" ).as< std::filesystem::path >();

   if( my->database_cfg.is_relative() )
      my->database_cfg = app().data_dir() / my->database_cfg;

   if( !std::filesystem::exists( my->database_cfg ) )
   {
      my->write_default_database_config( my->database_cfg );
   }
}

void chain_plugin::plugin_startup()
{
   // Check for state directory, and create if necessary
   if ( !std::filesystem::exists( my->state_dir ) ) { std::filesystem::create_directory( my->state_dir ); }

   nlohmann::json database_config;

   try
   {
      std::ifstream config_file( my->database_cfg, std::ios::binary );
      config_file >> database_config;
   }
   catch ( const std::exception& e )
   {
      LOG(error) << "error while parsing database configuration: " << e.what();
      exit( EXIT_FAILURE );
   }

   try
   {
      my->controller.open( my->state_dir, database_config );
   }
   catch( std::exception& e )
   {
      LOG(error) << "error opening database: " << e.what();
      exit( EXIT_FAILURE );
   }

   my->controller.start_threads();
}

void chain_plugin::plugin_shutdown()
{
   LOG(info) << "closing chain database";
   KOINOS_TODO( "We eventually need to call close() from somewhere" )
   //my->db.close();
   my->controller.stop_threads();
   LOG(info) << "database closed successfully";
}

} // namespace koinos::plugis::chain
