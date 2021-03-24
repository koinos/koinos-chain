#include <koinos/plugins/chain/chain_plugin.hpp>
#include <koinos/plugins/chain/reqhandler.hpp>

#include <koinos/log.hpp>
#include <koinos/mq/request_handler.hpp>
#include <koinos/mq/client.hpp>
#include <koinos/util.hpp>

#include <mira/database_configuration.hpp>

#include <koinos/pack/classes.hpp>
#include <koinos/pack/rt/binary.hpp>
#include <koinos/pack/rt/util/base58.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/util.hpp>

#include <chrono>

namespace koinos::plugins::chain {

using namespace appbase;

namespace detail {

class chain_plugin_impl
{
   public:
      chain_plugin_impl()  {}
      ~chain_plugin_impl() {}

      void write_default_database_config( bfs::path &p );

      void attach_client();
      void attach_request_handler();
      void reindex();

      bfs::path            state_dir;
      bfs::path            database_cfg;

      reqhandler           _reqhandler;

      bool                                   _reset = false;
      bool                                   _reindex = false;
      std::string                            _amqp_url;
      std::shared_ptr< mq::client >          _mq_client;
      std::shared_ptr< mq::request_handler > _mq_reqhandler;
      bool                                   _mq_disable = false;
      genesis_data                           _genesis_data;
};

void chain_plugin_impl::write_default_database_config( bfs::path &p )
{
   LOG(info) << "Writing database configuration: " << p.string();
   boost::filesystem::ofstream config_file( p, std::ios::binary );
   config_file << mira::utilities::default_database_configuration();
}

void chain_plugin_impl::reindex()
{
   using namespace rpc::block_store;
   try
   {
      constexpr uint64_t batch_size = 1000;
      const auto before = std::chrono::system_clock::now();

      LOG(info) << "Retrieving highest block";
      pack::json j;
      pack::to_json( j, block_store_request{ get_highest_block_request{} } );
      auto future = _mq_client->rpc( mq::service::block_store, j.dump() );

      block_store_response resp;
      pack::from_json( pack::json::parse( future.get() ), resp );

      block_topology target_head;
      std::visit( koinos::overloaded {
         [&]( get_highest_block_response& r )
         {
            target_head = r.topology;
         },
         [&]( block_store_error_response& e )
         {
            throw koinos::exception( e.error_text );
         },
         [&]( auto& )
         {
            throw koinos::exception( "unexpected block store response" );
         }
      }, resp );

      LOG(info) << "Reindexing to target block: " << target_head;

      multihash last_id = crypto::zero_hash( CRYPTO_SHA2_256_ID );
      block_height_type last_height = block_height_type{ 0 };

      get_blocks_by_height_request req {
         .head_block_id         = target_head.id,
         .ancestor_start_height = block_height_type{ 1 },
         .num_blocks            = batch_size,
         .return_block          = true,
         .return_receipt        = false
      };

      pack::to_json( j, block_store_request{ req } );
      future = _mq_client->rpc( mq::service::block_store, j.dump() );

      while ( last_id != target_head.id )
      {
         pack::from_json( pack::json::parse( future.get() ), resp );
         get_blocks_by_height_response batch;

         std::visit( koinos::overloaded {
            [&]( get_blocks_by_height_response& r )
            {
               batch = std::move( r );
            },
            [&]( block_store_error_response& e )
            {
               throw koinos::exception( e.error_text );
            },
            [&]( auto& )
            {
               throw koinos::exception( "unexpected block store response" );
            }
         }, resp );

         if ( !batch.block_items.empty() )
         {
            const auto& last_block_item = batch.block_items.back();

            if ( last_block_item.block.id != target_head.id )
            {
               get_blocks_by_height_request req {
                  .head_block_id         = target_head.id,
                  .ancestor_start_height = block_height_type{ last_block_item.block.header.height + 1 },
                  .num_blocks            = batch_size,
                  .return_block          = true,
                  .return_receipt        = false
               };

               pack::to_json( j, block_store_request{ req } );
               future = _mq_client->rpc( mq::service::block_store, j.dump() );
            }
         }

         for ( auto& block_item : batch.block_items )
         {
            types::rpc::block_submission args;
            args.block  = block_item.block;
            last_id     = block_item.block.id;
            last_height = block_item.block.header.height;

            _reqhandler.submit( args );
         }
      }

      _reqhandler.wait_for_jobs();

      const std::chrono::duration< double > duration = std::chrono::system_clock::now() - before;
      LOG(info) << "Finished reindexing " << last_height << " blocks, took " << duration.count() << " seconds";
   }
   catch ( const std::exception& e )
   {
      LOG(error) << "Reindex error: " << e.what();
      exit( EXIT_FAILURE );
   }
}

void chain_plugin_impl::attach_client()
{
   auto ec = _mq_client->connect( _amqp_url );

   if ( ec != mq::error_code::success )
   {
      LOG(error) << "Unable to connect AMQP client";
      exit( EXIT_FAILURE );
   }
}

void chain_plugin_impl::attach_request_handler()
{
   try
   {
      _reqhandler.set_client( _mq_client );
   }
   catch( std::exception& e )
   {
      LOG(error) << "Error connecting to AMQP server: " << e.what();
      exit( EXIT_FAILURE );
   }

   mq::error_code ec;

   ec = _mq_reqhandler->connect( _amqp_url );
   if ( ec != mq::error_code::success )
   {
      LOG(error) << "Unable to connect request handler to AMQP server";
      exit( EXIT_FAILURE );
   }

   ec = _mq_reqhandler->add_rpc_handler(
      mq::service::chain,
      [&]( const std::string& msg ) -> std::string
      {
         auto j = pack::json::parse( msg );
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
               [&]( const rpc::chain::get_fork_heads_request& p )
               {
                  submit = p;
               },
               [&]( const auto& p )
               {
                  submit = p;
               }
            },
            args
         );

         auto res = _reqhandler.submit( submit );

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
                        r.get_const_native()
                     );
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
            *(res.get())
         );

         pack::to_json( j, result );
         return j.dump();
      }
   );

   if ( ec != mq::error_code::success )
   {
      LOG(error) << "unable to prepare mq server for processing";
      exit( EXIT_FAILURE );
   }

   _mq_reqhandler->add_broadcast_handler(
      "koinos.block.accept",
      [&]( const std::string& msg )
      {
         broadcast::block_accepted bam;
         pack::from_json( pack::json::parse( msg ), bam );

         types::rpc::block_submission args;
         args.block = bam.block;
         _reqhandler.submit( args );
      }
   );

   if ( ec != mq::error_code::success )
   {
      LOG(error) << "unable to prepare mq server for processing";
      exit( EXIT_FAILURE );
   }

   _mq_reqhandler->start();
}

} // detail


chain_plugin::chain_plugin() : my( new detail::chain_plugin_impl() ) {}
chain_plugin::~chain_plugin(){}

bfs::path chain_plugin::state_dir() const
{
   return my->state_dir;
}

std::string get_default_chain_id_string()
{
   // Following is equivalent to {"digest":"z5gosJRaEAWdexTCiVqmjDECb7odR7SrvsNLWxG5NBKhx","hash":18}
   return "zQmT2TaQZZjwW7HZ6ctY3VCsPvadHV1m6RcwgMNeRUgP1mx";
}

void chain_plugin::set_program_options( options_description& cli, options_description& cfg )
{
   cfg.add_options()
         ("state-dir", bpo::value<bfs::path>()->default_value("blockchain"),
            "the location of the blockchain state files (absolute path or relative to application data dir)")
         ("database-config", bpo::value<bfs::path>()->default_value("database.cfg"), "The database configuration file location")
         ("amqp", bpo::value<std::string>()->default_value("amqp://guest:guest@localhost:5672/"), "AMQP server URL")
         ("mq-disable", bpo::value<bool>()->default_value(false), "Disables MQ connection")
         ("chain-id", bpo::value<std::string>()->default_value(get_default_chain_id_string()), "Chain ID to initialize empty node state")
         ;
   cli.add_options()
         ("reset", bpo::bool_switch()->default_value(false), "reset the database")
         ("reindex", bpo::bool_switch()->default_value(false), "Recreate chain state from the block store")
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

   if ( options.count( "reset" ) )
   {
      my->_reset = options.at("reset").as< bool >();
   }

   if ( options.count( "reindex" ) )
   {
      my->_reindex = options.at( "reindex" ).as< bool >();

      // If we are to reindex, we must begin with a reset
      if ( my->_reindex )
         my->_reset = true;
   }

   std::string chain_id_str = options.at("chain-id").as< std::string >();

   KOINOS_ASSERT(
      chain_id_str.size() > 0 && chain_id_str[0] == 'z',
      exception,
      "expected chain id to be a base58 string prefixed with 'z'"
   );

   variable_blob chain_id_blob;
   KOINOS_ASSERT(
      pack::util::decode_base58( chain_id_str.c_str() + 1, chain_id_blob ),
      exception,
      "failed to decode chain id digest"
   );
   multihash chain_id = pack::from_variable_blob< multihash >(chain_id_blob);
   my->_genesis_data[ KOINOS_STATEDB_CHAIN_ID_KEY ] = pack::to_variable_blob( chain_id );
}

void chain_plugin::plugin_startup()
{
   // Check for state directory, and create if necessary
   if ( !bfs::exists( my->state_dir ) ) { bfs::create_directory( my->state_dir ); }

   pack::json database_config;

   try
   {
      boost::filesystem::ifstream config_file( my->database_cfg, std::ios::binary );
      config_file >> database_config;
   }
   catch ( const std::exception& e )
   {
      LOG(error) << "Error while parsing database configuration: " << e.what();
      exit( EXIT_FAILURE );
   }

   try
   {
      my->_reqhandler.open( my->state_dir, database_config, my->_genesis_data, my->_reset );
   }
   catch( std::exception& e )
   {
      LOG(error) << "Error opening database: " << e.what();
      exit( EXIT_FAILURE );
   }

   my->_reqhandler.start_threads();

   my->_mq_client = std::make_shared< mq::client >();
   my->_mq_reqhandler = std::make_shared< mq::request_handler >();

   if ( !my->_mq_disable )
   {
      my->attach_client();
      LOG(info) << "Connected to AMQP server";

      if ( my->_reindex )
      {
         LOG(info) << "Recreating chain state...";
         my->reindex();
      }

      my->attach_request_handler();
      LOG(info) << "Listening for requests over AMQP";
   }
   else
   {
      LOG(warning) << "Application is running without AMQP support";
   }
}

void chain_plugin::plugin_shutdown()
{
   LOG(info) << "Closing chain database";

   if ( !my->_mq_disable )
   {
      LOG(info) << "Closing AMQP request handler";
      my->_mq_reqhandler->stop();
   }

   my->_reqhandler.stop_threads();
   LOG(info) << "Database closed successfully";
}

std::future< std::shared_ptr< koinos::types::rpc::submission_result > > chain_plugin::submit( const koinos::types::rpc::submission_item& item )
{
   return my->_reqhandler.submit( item );
}

} // namespace koinos::plugis::chain
