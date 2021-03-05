#include <koinos/plugins/chain/chain_plugin.hpp>
#include <koinos/plugins/chain/reqhandler.hpp>

#include <koinos/log.hpp>
#include <koinos/mq/request_handler.hpp>
#include <koinos/util.hpp>

#include <mira/database_configuration.hpp>

#include <koinos/pack/classes.hpp>
#include <koinos/pack/rt/binary.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/util.hpp>

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

      reqhandler           _reqhandler;

      bool                                   _mq_disable = false;
      bool                                   _reset = false;
      std::string                            _amqp_url;
      std::shared_ptr< mq::request_handler > _mq_reqhandler;
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
         ("amqp", bpo::value<std::string>()->default_value("amqp://guest:guest@localhost:5672/"), "AMQP server URL")
         ("mq-disable", bpo::value<bool>()->default_value(false), "Disables MQ connection")
         ;
   cli.add_options()
         ("reset", bpo::bool_switch()->default_value(false), "reset the database");
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

   my->_amqp_url = options.at( "amqp" ).as< std::string >();
   my->_mq_disable = options.at("mq-disable").as< bool >();
   my->_reset = options.at("reset").as< bool >();
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
      my->_reqhandler.open( my->state_dir, database_config, my->_reset );
   }
   catch( std::exception& e )
   {
      LOG(error) << "error opening database: " << e.what();
      exit( EXIT_FAILURE );
   }

   my->_reqhandler.start_threads();

   my->_mq_reqhandler = std::make_shared< mq::request_handler >();

   if ( !my->_mq_disable )
   {
      try
      {
         my->_reqhandler.connect( my->_amqp_url );
      }
      catch( std::exception& e )
      {
         LOG(error) << "error connecting to mq server: " << e.what();
         exit( EXIT_FAILURE );
      }

      mq::error_code ec;

      ec = my->_mq_reqhandler->connect( my->_amqp_url );
      if ( ec != mq::error_code::success )
      {
         LOG(error) << "unable to connect request handler to mq server";
         exit( EXIT_FAILURE );
      }

      ec = my->_mq_reqhandler->add_msg_handler(
         "koinos_rpc",
         "koinos_rpc_chain",
         true,
         []( const std::string& content_type ) { return content_type == "application/json"; },
         [&]( const std::string& msg ) -> std::string
         {
            auto j = nlohmann::json::parse( msg );
            rpc::chain::chain_rpc_request args;
            pack::from_json( j, args );

            // Convert chain_rpc_params -> submission_item
            types::rpc::submission_item submit;
            std::visit(
               koinos::overloaded {
                  [&]( const rpc::chain::get_head_info_request& p )
                  {
                     submit = types::rpc::query_param_item( p );
                  },
                  [&]( const rpc::chain::get_chain_id_request& p )
                  {
                     submit = types::rpc::query_param_item( p );
                  },
                  [&]( const rpc::chain::get_pending_transactions_request& p )
                  {
                     submit = types::rpc::query_param_item( p );
                  },
                  [&]( const rpc::chain::get_fork_heads_request& p )
                  {
                     submit = p;
                  },
                  [&]( const auto& p )
                  {
                     submit = p;
                  }
               },
               args );

            auto res = my->_reqhandler.submit( submit );

            // Convert submission_result -> chain_rpc_result
            rpc::chain::chain_rpc_response result;
            std::visit(
               koinos::overloaded {
                  [&]( const types::rpc::query_submission_result& r )
                  {
                     r.unbox();
                     if ( r.is_unboxed() )
                     {
                        std::visit(
                           koinos::overloaded {
                              [&]( const auto& q )
                              {
                                 result = q;
                              }
                           },
                        r.get_const_native() );
                     }
                     else
                     {
                        result = rpc::chain::chain_error_response{ "Unknown serialization returned for query submission result" };
                     }
                  },
                  [&]( const auto& r )
                  {
                     result = r;
                  }
               },
               *(res.get()) );

            pack::to_json( j, result );
            return j.dump();
         }
      );

      if ( ec != mq::error_code::success )
      {
         LOG(error) << "unable to prepare mq server for processing";
         exit( EXIT_FAILURE );
      }

      my->_mq_reqhandler->add_msg_handler(
         "koinos_event",
         "koinos.block.accept",
         false,
         []( const std::string& content_type ) { return content_type == "application/json"; },
         [&]( const std::string& msg )
         {
            broadcast::block_accepted bam;
            pack::from_json( nlohmann::json::parse( msg ), bam );

            types::rpc::block_submission args;
            args.topology = bam.topology;
            args.block    = bam.block;
            my->_reqhandler.submit( args );
         }
      );

      if ( ec != mq::error_code::success )
      {
         LOG(error) << "unable to prepare mq server for processing";
         exit( EXIT_FAILURE );
      }

      my->_mq_reqhandler->start();

   }
   else
   {
      LOG(warning) << "application is running without MQ support";
   }
}

void chain_plugin::plugin_shutdown()
{
   LOG(info) << "closing chain database";

   if ( !my->_mq_disable )
   {
      LOG(info) << "closing mq request handler";
      my->_mq_reqhandler->stop();
   }

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
