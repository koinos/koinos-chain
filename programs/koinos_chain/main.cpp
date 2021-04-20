#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <boost/thread/sync_bounded_queue.hpp>

#include <yaml-cpp/yaml.h>

#include <koinos/chain/controller.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/exception.hpp>
#include <koinos/mq/client.hpp>
#include <koinos/mq/request_handler.hpp>
#include <koinos/log.hpp>
#include <koinos/util.hpp>

#include <mira/database_configuration.hpp>

#define KOINOS_MAJOR_VERSION "0"
#define KOINOS_MINOR_VERSION "1"
#define KOINOS_PATCH_VERSION "0"

#define HELP_OPTION             "help"
#define VERSION_OPTION          "version"
#define BASEDIR_OPTION          "basedir"
#define AMQP_OPTION             "amqp"
#define AMQP_DEFAULT            "amqp://guest:guest@localhost:5672/"
#define LOG_LEVEL_OPTION        "log-level"
#define LOG_LEVEL_DEFAULT       "info"
#define INSTANCE_ID_OPTION      "instance-id"
#define STATEDIR_OPTION         "statedir"
#define STATEDIR_DEFAULT        "blockchain"
#define DATABASE_CONFIG_OPTION  "database-config"
#define DATABASE_CONFIG_DEFAULT "database.cfg"
#define CHAIN_ID_OPTION         "chain-id"
#define RESET_OPTION            "reset"

using namespace boost;
using namespace koinos;

constexpr uint32_t MAX_AMQP_CONNECT_SLEEP_MS = 30000;

const std::string& version_string()
{
   static std::string v_str = "Koinos chain v" KOINOS_MAJOR_VERSION "." KOINOS_MINOR_VERSION "." KOINOS_PATCH_VERSION;
   return v_str;
}

void splash()
{
   const char* banner = R"BANNER(
  _  __     _
 | |/ /___ (_)_ __   ___  ___
 | ' // _ \| | '_ \ / _ \/ __|
 | . \ (_) | | | | | (_) \__ \
 |_|\_\___/|_|_| |_|\___/|___/)BANNER";

   std::cout.write( banner, std::strlen( banner ) );
   std::cout << std::endl;
   const char* launch_message = "          ...launching network";
   std::cout.write( launch_message, std::strlen( launch_message ) );
   std::cout << std::endl << std::flush;
}

std::string get_default_chain_id_string()
{
   // Following is equivalent to {"digest":"z5gosJRaEAWdexTCiVqmjDECb7odR7SrvsNLWxG5NBKhx","hash":18}
   return "zQmT2TaQZZjwW7HZ6ctY3VCsPvadHV1m6RcwgMNeRUgP1mx";
}

void write_default_database_config( const std::filesystem::path& p )
{
   LOG(info) << "Writing database configuration: " << p.string();
   std::ofstream config_file( p, std::ios::binary );
   config_file << mira::utilities::default_database_configuration();
}

void attach_client(
   chain::controller& controller,
   std::shared_ptr< mq::client > mq_client,
   const std::string& amqp_url )
{
   try
   {
      controller.set_client( mq_client );
   }
   catch( std::exception& e )
   {
      LOG(error) << "Error connecting to AMQP server: " << e.what();
      exit( EXIT_FAILURE );
   }
}

void attach_request_handler(
   chain::controller& controller,
   mq::request_handler& mq_reqhandler,
   const std::string& amqp_url )
{
   mq::error_code ec = mq::error_code::success;

   ec = mq_reqhandler.add_rpc_handler(
      service::chain,
      [&]( const std::string& msg ) -> std::string
      {
         rpc::chain::chain_rpc_response response;
         pack::json j;

         try
         {
            j = pack::json::parse( msg );
            rpc::chain::chain_rpc_request request;
            pack::from_json( j, request );

            std::visit(
               koinos::overloaded {
                  [&]( const rpc::chain::submit_block_request& r )
                  {
                     response = controller.submit_block( r );
                  },
                  [&]( const rpc::chain::submit_transaction_request& r )
                  {
                     response = controller.submit_transaction( r );
                  },
                  [&]( const rpc::chain::get_head_info_request& r )
                  {
                     response = controller.get_head_info( r );
                  },
                  [&]( const rpc::chain::get_chain_id_request& r )
                  {
                     response = controller.get_chain_id( r );
                  },
                  [&]( const rpc::chain::get_fork_heads_request& r )
                  {
                     response = controller.get_fork_heads( r );
                  },
                  [&]( const auto& r )
                  {
                     response = rpc::chain::chain_error_response{ .error_text = "Unknown RPC type" };
                  }
               },
               request
            );
         }
         catch( koinos::exception& e )
         {
            response = rpc::chain::chain_error_response {
               .error_text = e.get_message(),
               .error_data = e.get_stacktrace()
            };
         }
         catch( std::exception& e )
         {
            response = rpc::chain::chain_error_response {
               .error_text = e.what()
            };
         }

         j.clear();
         pack::to_json( j, response );
         return j.dump();
      }
   );

   if ( ec != mq::error_code::success )
   {
      LOG(error) << "Unable to register MQ RPC handler";
      exit( EXIT_FAILURE );
   }

   ec = mq_reqhandler.add_broadcast_handler(
      "koinos.block.accept",
      [&]( const std::string& msg )
      {
         try {
            broadcast::block_accepted bam;
            pack::from_json( pack::json::parse( msg ), bam );

            controller.submit_block( {
               .block = bam.block,
               .verify_passive_data = false,
               .verify_block_signature = false,
               .verify_transaction_signatures = false
            } );
         }
         catch( const boost::exception& e )
         {
            LOG(warning) << "Error handling block broadcast: " << boost::diagnostic_information( e );
         }
         catch( const std::exception& e )
         {
            LOG(warning) << "Error handling block broadcast: " << e.what();
         }
      }
   );

   if ( ec != mq::error_code::success )
   {
      LOG(error) << "Unable to register block broadcast handler";
      exit( EXIT_FAILURE );
   }

   LOG(info) << "Connecting AMQP request handler...";
   ec = mq_reqhandler.connect( amqp_url );
   if ( ec != mq::error_code::success )
   {
      LOG(error) << "Failed to connect request handler to AMQP server";
      exit( EXIT_FAILURE );
   }
   LOG(info) << "Established request handler connection to the AMQP server";

   mq_reqhandler.start();
}


void index_loop(
   chain::controller& controller,
   concurrent::sync_bounded_queue< std::shared_future< std::string > >& rpc_queue )
{
   while( true )
   {
      std::shared_future< std::string > future;
      try
      {
         rpc_queue.pull_front( future );
      }
      catch ( const concurrent::sync_queue_is_closed& )
      {
         break;
      }

      try
      {
         rpc::block_store::block_store_response resp;
         pack::from_json( pack::json::parse( future.get() ), resp );
         rpc::block_store::get_blocks_by_height_response batch;

         std::visit( koinos::overloaded {
            [&]( rpc::block_store::get_blocks_by_height_response& r )
            {
               batch = std::move( r );
            },
            [&]( rpc::block_store::block_store_error_response& e )
            {
               throw koinos::exception( e.error_text );
            },
            [&]( auto& )
            {
               throw koinos::exception( "unexpected block store response" );
            }
         }, resp );

         for ( auto& block_item : batch.block_items )
         {
            controller.submit_block( {
               .block = block_item.block.get_const_native(),
               .verify_passive_data = false,
               .verify_block_signature = false,
               .verify_transaction_signatures = false
            }, true );
         }
      }
      catch ( const boost::exception& e )
      {
         LOG(error) << "Index error: " << boost::diagnostic_information( e );
         exit( EXIT_FAILURE );
      }
      catch ( const std::exception& e )
      {
         LOG(error) << "Index error: " << e.what();
         exit( EXIT_FAILURE );
      }
   }
}

void index( chain::controller& controller, std::shared_ptr< mq::client > mq_client )
{
   using namespace rpc::block_store;
   try
   {
      constexpr uint64_t batch_size = 1000;
      const auto before = std::chrono::system_clock::now();

      LOG(info) << "Retrieving highest block from block store";
      pack::json j;
      pack::to_json( j, block_store_request{ get_highest_block_request{} } );
      auto future = mq_client->rpc( service::block_store, j.dump() );

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

      auto head_info = controller.get_head_info();

      if( head_info.head_topology.height < target_head.height )
      {
         LOG(info) << "Indexing to target block: " << target_head;

         concurrent::sync_bounded_queue< std::shared_future< std::string > > rpc_queue{ 10 };

         auto index_thread = std::make_unique< std::thread >( [&]()
         {
            index_loop( controller, rpc_queue );
         } );

         multihash last_id = crypto::zero_hash( CRYPTO_SHA2_256_ID );
         block_height_type last_height = head_info.head_topology.height;

         while ( last_height < target_head.height )
         {
            get_blocks_by_height_request req {
               .head_block_id         = target_head.id,
               .ancestor_start_height = block_height_type{ last_height + 1 },
               .num_blocks            = batch_size,
               .return_block          = true,
               .return_receipt        = false
            };

            pack::to_json( j, block_store_request{ req } );
            rpc_queue.push_back( mq_client->rpc( service::block_store, j.dump() ) );
            last_height += block_height_type{ batch_size };
         }

         rpc_queue.close();
         index_thread->join();

         auto new_head_info = controller.get_head_info();

         const std::chrono::duration< double > duration = std::chrono::system_clock::now() - before;
         LOG(info) << "Finished indexing " << new_head_info.head_topology.height - head_info.head_topology.height << " blocks, took " << duration.count() << " seconds";
      }
   }
   catch ( const std::exception& e )
   {
      LOG(error) << "Index error: " << e.what();
      exit( EXIT_FAILURE );
   }
}

template< typename T >
T get_option(
   std::string key,
   T default_value,
   const program_options::variables_map& cli_args,
   const YAML::Node& service_config = YAML::Node(),
   const YAML::Node& global_config = YAML::Node() )
{
   if ( cli_args.count( key ) )
      return cli_args[ key ].as< T >();

   if ( service_config && service_config[ key ] )
      return service_config[ key ].as< T >();

   if ( global_config && global_config[ key ] )
      return global_config[ key ].as< T >();

   return std::move( default_value );
}

int main( int argc, char** argv )
{
   try
   {
      program_options::options_description options;
      options.add_options()
         (HELP_OPTION       ",h", "Print this help message and exit")
         (VERSION_OPTION    ",v", "Print version string and exit")
         (BASEDIR_OPTION    ",d", program_options::value< std::string >()->default_value( get_default_base_directory().string() ), "Koinos base directory")
         (AMQP_OPTION       ",a", program_options::value< std::string >(), "AMQP server URL")
         (LOG_LEVEL_OPTION  ",l", program_options::value< std::string >(), "The log filtering level")
         (INSTANCE_ID_OPTION",i", program_options::value< std::string >(), "An ID that uniquely identifies the instance")
         (STATEDIR_OPTION       , program_options::value< std::string >(),
            "The location of the blockchain state files (absolute path or relative to basedir/chain)")
         (DATABASE_CONFIG_OPTION, program_options::value< std::string >(),
            "The location of the database configuration file (absolute path or relative to basedir/chain)")
         (CHAIN_ID_OPTION       , program_options::value< std::string >(), "Chain ID to initialize empty node state")
         (RESET_OPTION          , program_options::bool_switch()->default_value(false), "Reset the database");

      program_options::variables_map args;
      program_options::store( program_options::parse_command_line( argc, argv, options ), args );

      if ( args.count( HELP_OPTION ) )
      {
         std::cout << options << std::endl;
         return EXIT_SUCCESS;
      }

      if ( args.count( VERSION_OPTION ) )
      {
         const auto& v_str = version_string();
         std::cout.write( v_str.c_str(), v_str.size() );
         std::cout << std::endl;
         return EXIT_SUCCESS;
      }

      splash();

      auto basedir = std::filesystem::path( args[ BASEDIR_OPTION ].as< std::string >() );
      if ( basedir.is_relative() )
         basedir = std::filesystem::current_path() / basedir;

      YAML::Node config;
      YAML::Node global_config;
      YAML::Node chain_config;

      auto yaml_config = basedir / "config.yml";
      if ( !std::filesystem::exists( yaml_config ) )
      {
         yaml_config = basedir / "config.yaml";
      }

      if ( std::filesystem::exists( yaml_config ) )
      {
         config = YAML::LoadFile( yaml_config );
         global_config = config[ "global" ];
         chain_config = config[ service::chain ];
      }

      std::string amqp_url      = get_option< std::string >( AMQP_OPTION, AMQP_DEFAULT, args, chain_config, global_config );
      std::string log_level     = get_option< std::string >( LOG_LEVEL_OPTION, LOG_LEVEL_DEFAULT, args, chain_config, global_config );
      std::string instance_id   = get_option< std::string >( INSTANCE_ID_OPTION, random_alphanumeric( 5 ), args, chain_config, global_config );
      auto statedir             = std::filesystem::path( get_option< std::string >( STATEDIR_OPTION, STATEDIR_DEFAULT, args, chain_config ) );
      auto database_config_path = std::filesystem::path( get_option< std::string >( DATABASE_CONFIG_OPTION, DATABASE_CONFIG_DEFAULT, args, chain_config ) );
      auto chain_id_str         = get_option< std::string >( CHAIN_ID_OPTION, get_default_chain_id_string(), args, chain_config );

      koinos::initialize_logging( service::chain, instance_id, log_level, basedir / service::chain );

      if ( config.IsNull() )
      {
         LOG(warning) << "Could not find config (config.yml or config.yaml expected). Using default values";
      }

      if ( statedir.is_relative() )
         statedir = basedir / service::chain / statedir;

      if ( !std::filesystem::exists( statedir ) )
         std::filesystem::create_directories( statedir );

      if ( database_config_path.is_relative() )
         database_config_path = basedir / service::chain / database_config_path;

      if ( !std::filesystem::exists( database_config_path ) )
         write_default_database_config( database_config_path );

      pack::json database_config;

      try
      {
         std::ifstream config_file( database_config_path, std::ios::binary );
         config_file >> database_config;
      }
      catch ( const std::exception& e )
      {
         LOG(error) << "Error while parsing database configuration: " << e.what();
         exit( EXIT_FAILURE );
      }

      multihash chain_id;
      try
      {
         pack::from_json( pack::json( chain_id_str ), chain_id );
      }
      catch ( const std::exception& e )
      {
         LOG(error) << "Error parsing chain id: " << e.what();
         exit( EXIT_FAILURE );
      }


      chain::genesis_data genesis_data;
      genesis_data[ KOINOS_STATEDB_CHAIN_ID_KEY ] = pack::to_variable_blob( chain_id );

      chain::controller controller;
      controller.open( statedir, database_config, genesis_data, args[ RESET_OPTION ].as< bool >() );

      auto mq_client = std::make_shared< mq::client >();
      auto request_handler = mq::request_handler();

      LOG(info) << "Connecting AMQP client...";
      auto ec = mq_client->connect( amqp_url );
      if ( ec != mq::error_code::success )
      {
         LOG(error) << "Failed to connect AMQP client to server" ;
         exit( EXIT_FAILURE );
      }
      LOG(info) << "Established AMQP client connection to the server";

      {
         LOG(info) << "Attempting to connect to block_store...";
         pack::json j;
         pack::to_json( j, rpc::block_store::block_store_request{ rpc::block_store::block_store_reserved_request{} } );
         mq_client->rpc( service::block_store, j.dump() ).get();
         LOG(info) << "Established connection to block_store";
      }

      {
         LOG(info) << "Attempting to connect to mempool...";
         pack::json j;
         pack::to_json( j, rpc::mempool::mempool_rpc_request{ rpc::mempool::mempool_reserved_request{} } );
         mq_client->rpc( service::mempool, j.dump() ).get();
         LOG(info) << "Established connection to mempool";
      }

      index( controller, mq_client );

      attach_client( controller, mq_client, amqp_url );
      attach_request_handler( controller, request_handler, amqp_url );
      LOG(info) << "Listening for requests over AMQP";

      asio::io_service io_service;
      asio::signal_set signals( io_service, SIGINT, SIGTERM );

      signals.async_wait( [&]( const system::error_code& err, int num )
      {
         LOG(info) << "Caught signal, shutting down...";
         request_handler.stop();
      } );

      io_service.run();
      LOG(info) << "Shut down successfully";

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
      LOG(fatal) << "Unknown exception" << std::endl;
   }

   return EXIT_FAILURE;
}
