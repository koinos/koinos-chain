#include <koinos/plugins/chain/chain_plugin.hpp>
#include <koinos/plugins/chain/reqhandler.hpp>

#include <koinos/log.hpp>
#include <koinos/util.hpp>

#include <mira/database_configuration.hpp>

namespace koinos::plugins::chain {

using namespace appbase;

namespace detail {

class chain_plugin_impl
{
   public:
      chain_plugin_impl()  {}
      ~chain_plugin_impl() {}

      void write_default_database_config( bfs::path &p );

      bfs::path            state_dir;
      bfs::path            database_cfg;

      std::string          mq_host;
      uint16_t             mq_port;
      std::string          mq_vhost;
      std::string          mq_user;
      std::string          mq_pass;

      reqhandler           _reqhandler;
};

void chain_plugin_impl::write_default_database_config( bfs::path &p )
{
   LOG(info) << "writing database configuration: " << p.string();
   boost::filesystem::ofstream config_file( p, std::ios::binary );
   config_file << mira::utilities::default_database_configuration();
}

} // detail


chain_plugin::chain_plugin() : my( new detail::chain_plugin_impl() ) {}
chain_plugin::~chain_plugin(){}

bfs::path chain_plugin::state_dir() const
{
   return my->state_dir;
}

void chain_plugin::set_program_options( options_description& cli, options_description& cfg )
{
   cfg.add_options()
         ("state-dir", bpo::value<bfs::path>()->default_value("blockchain"),
            "the location of the blockchain state files (absolute path or relative to application data dir)")
         ("database-config", bpo::value<bfs::path>()->default_value("database.cfg"), "The database configuration file location")
         ("mq-host", bpo::value<std::string>()->default_value("localhost"), "The MQ server host")
         ("mq-port", bpo::value<uint16_t>()->default_value(5672), "The MQ server port")
         ("mq-vhost", bpo::value<std::string>()->default_value("/"), "The MQ server virtual host")
         ("mq-user", bpo::value<std::string>()->default_value("guest"), "The MQ server username")
         ("mq-pass", bpo::value<std::string>()->default_value("guest"), "The MQ server password")
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
      auto sfd = options.at("state-dir").as<bfs::path>();
      if(sfd.is_relative())
         my->state_dir = app().data_dir() / sfd;
      else
         my->state_dir = sfd;
   }

   my->database_cfg = options.at( "database-config" ).as< bfs::path >();

   if( my->database_cfg.is_relative() )
      my->database_cfg = app().data_dir() / my->database_cfg;

   if( !bfs::exists( my->database_cfg ) )
   {
      my->write_default_database_config( my->database_cfg );
   }

   my->mq_host = options.at("mq-host").as< std::string >();
   my->mq_port = options.at("mq-port").as< uint16_t >();
   my->mq_vhost = options.at("mq-vhost").as< std::string >();
   my->mq_user = options.at("mq-user").as< std::string >();
   my->mq_pass = options.at("mq-pass").as< std::string >();
}

void chain_plugin::plugin_startup()
{
   // Check for state directory, and create if necessary
   if ( !bfs::exists( my->state_dir ) ) { bfs::create_directory( my->state_dir ); }

   nlohmann::json database_config;

   try
   {
      boost::filesystem::ifstream config_file( my->database_cfg, std::ios::binary );
      config_file >> database_config;
   }
   catch ( const std::exception& e )
   {
      LOG(error) << "error while parsing database configuration: " << e.what();
      exit( EXIT_FAILURE );
   }

   try
   {
      my->_reqhandler.open( my->state_dir, database_config );
   }
   catch( std::exception& e )
   {
      LOG(error) << "error opening database: " << e.what();
      exit( EXIT_FAILURE );
   }

   try
   {
      my->_reqhandler.connect( my->mq_host, my->mq_port, my->mq_vhost, my->mq_user, my->mq_pass );
   }
   catch( std::exception& e )
   {
      LOG(error) << "error connecting to mq server: " << e.what();
      exit( EXIT_FAILURE );
   }

   my->_reqhandler.start_threads();
}

void chain_plugin::plugin_shutdown()
{
   LOG(info) << "closing chain database";
   KOINOS_TODO( "We eventually need to call close() from somewhere" )
   //my->db.close();
   my->_reqhandler.stop_threads();
   LOG(info) << "database closed successfully";
}

std::future< std::shared_ptr< koinos::types::rpc::submission_result > > chain_plugin::submit( const koinos::types::rpc::submission_item& item )
{
   return my->_reqhandler.submit( item );
}


} // namespace koinos::plugis::chain
