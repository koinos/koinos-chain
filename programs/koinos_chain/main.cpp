#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>


#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/filesystem.hpp>
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

#define HELP_OPTION            "help"
#define BASEDIR_OPTION         "basedir"
#define VERSION_OPTION         "version"
#define MQ_DISABLE_OPTION      "mq-disable"
#define RESET_OPTION           "reset"

#define AMQP_OPTION            "amqp"
#define AMQP_DEFAULT           "amqp://guest:guest@localhost:5672/"
#define STATEDIR_OPTION        "statedir"
#define STATEDIR_DEFAULT       "blockchain"
#define DATABASE_CONFIG_OPTION "database-config"
#define DATABASE_CONFIG_DEFAULT "database.cfg"
#define CHAIN_ID_OPTION        "chain-id"

using namespace boost;
using namespace koinos;

constexpr uint32_t MAX_AMQP_CONNECT_SLEEP_MS = 30000;

const std::string& version_string()
{
   static std::string v_str = "Koinos Chain v" KOINOS_MAJOR_VERSION "." KOINOS_MINOR_VERSION "." KOINOS_PATCH_VERSION;
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

   std::cout.write( BANNER, strlen( BANNER ) );
#ifdef IS_TEST_NET
   const char* LAUNCH_MESSAGE = "       ...launching test network";
#else
   const char* LAUNCH_MESSAGE = "       ...launching main network";
#endif
   std::cout.write( LAUNCH_MESSAGE, strlen( LAUNCH_MESSAGE ) );
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
   uint32_t amqp_sleep_ms = 1000;

   LOG(info) << "Connecting AMQP request handler...";
   while ( true )
   {
      auto ec = mq_reqhandler.connect( amqp_url );
      if ( ec == mq::error_code::success )
      {
         LOG(info) << "Connected request handler to AMQP server";
         break;
      }
      else
      {
         LOG(info) << "Failed, trying again in " << amqp_sleep_ms << " ms" ;
         std::this_thread::sleep_for( std::chrono::milliseconds( amqp_sleep_ms ) );
         amqp_sleep_ms = std::min( amqp_sleep_ms * 2, MAX_AMQP_CONNECT_SLEEP_MS );
      }
   }

   auto ec = mq_reqhandler.add_rpc_handler(
      mq::service::chain,
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

   mq_reqhandler.add_broadcast_handler(
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
      auto future = mq_client->rpc( mq::service::block_store, j.dump() );

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
            rpc_queue.push_back( mq_client->rpc( mq::service::block_store, j.dump() ) );
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

int main( int argc, char** argv )
{
   try
   {
      program_options::options_description options;
      options.add_options()
         (HELP_OPTION       ",h", "Print this help message and exit")
         (VERSION_OPTION    ",v", "Print version string and exit")
         (BASEDIR_OPTION    ",d", program_options::value< std::string >()->default_value( get_default_base_directory().string() ), "Koinos base directory")
         (MQ_DISABLE_OPTION     , program_options::bool_switch()->default_value(false), "Disables MQ connection")
         (RESET_OPTION          , program_options::bool_switch()->default_value(false), "reset the database");

      program_options::variables_map args;
      program_options::store( program_options::parse_command_line( argc, argv, options ), args );

      if ( args.count( HELP_OPTION ) )
      {
         std::cout << options << std::endl;
         return EXIT_FAILURE;
      }

      if ( args.count( VERSION_OPTION ) )
      {
         const auto& v_str = version_string();
         std::cout.write( v_str.c_str(), v_str.size() );
         std::cout << std::endl;
         return EXIT_FAILURE;
      }

      splash();

      auto basedir = std::filesystem::path( args[ BASEDIR_OPTION ].as< std::string >() );
      if ( basedir.is_relative() )
         basedir = std::filesystem::current_path() / basedir;

      koinos::initialize_logging( boost::filesystem::path( basedir.string() ), "chain/log/%3N.log" );

      std::string amqp_url = AMQP_DEFAULT;
      auto statedir = std::filesystem::path( STATEDIR_DEFAULT );
      auto database_config_path = std::filesystem::path( DATABASE_CONFIG_DEFAULT );
      auto chain_id_str = get_default_chain_id_string();

      auto yaml_config = basedir / "config.yml";
      if ( !std::filesystem::exists( yaml_config ) )
      {
         yaml_config = basedir / "config.yaml";
      }

      if ( !std::filesystem::exists( yaml_config ) )
      {
         LOG(warning) << "Could not find config (config.yml or config.yaml expected). Using default values";
      }
      else
      {
         YAML::Node config = YAML::LoadFile( yaml_config );

         if ( config[mq::service::chain] )
         {
            const auto& chain_node = config[mq::service::chain];
            if ( chain_node[ AMQP_OPTION ] )
            {
               amqp_url = chain_node[ AMQP_OPTION ].as< std::string >();
            }

            if ( chain_node[ STATEDIR_OPTION ] )
            {
               statedir = std::filesystem::path( chain_node[ STATEDIR_OPTION ].as< std::string >() );
            }

            if ( chain_node[ DATABASE_CONFIG_OPTION ] )
            {
               database_config_path = std::filesystem::path( chain_node[ DATABASE_CONFIG_OPTION ].as< std::string >() );
            }

            if ( chain_node[ CHAIN_ID_OPTION ] )
            {
               chain_id_str = chain_node[ CHAIN_ID_OPTION ].as< std::string >();
            }
         }
         else
         {
            LOG(warning) << "Could not find config for microservice " << mq::service::chain
               << ". Using default values";
         }
      }

      if ( statedir.is_relative() )
         statedir = basedir / "chain" / statedir;

      if ( !std::filesystem::exists( statedir ) )
         std::filesystem::create_directories( statedir );

      if ( database_config_path.is_relative() )
         database_config_path = basedir / "chain" / database_config_path;

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
      controller.open( boost::filesystem::path( statedir.string() ), database_config, genesis_data, args[ RESET_OPTION ].as< bool >() );

      auto mq_client = std::make_shared< mq::client >();
      auto request_handler = mq::request_handler();

      if ( !args[ MQ_DISABLE_OPTION ].as< bool >() )
      {
         uint32_t amqp_sleep_ms = 1000;

         LOG(info) << "Connecting AMQP client...";
         while ( true )
         {
            auto ec = mq_client->connect( amqp_url );
            if ( ec == mq::error_code::success )
            {
               LOG(info) << "Connected client to AMQP server";
               break;
            }
            else
            {
               LOG(info) << "Failed, trying again in " << amqp_sleep_ms << " ms" ;
               std::this_thread::sleep_for( std::chrono::milliseconds( amqp_sleep_ms ) );
               amqp_sleep_ms = std::min( amqp_sleep_ms * 2, MAX_AMQP_CONNECT_SLEEP_MS );
            }
         }

         LOG(info) << "Attempting to connect to block_store...";
         while ( true )
         {
            KOINOS_TODO("Remove this loop when MQ client retry logic is implemented (koinos-mq-cpp#15)")
            pack::json j;
            pack::to_json( j, rpc::block_store::block_store_request{ rpc::block_store::block_store_reserved_request{} } );

            try
            {
               mq_client->rpc( mq::service::block_store, j.dump() ).get();
               LOG(info) << "Connected";
               break;
            }
            catch( const mq::timeout_error& ) {}
         }

         LOG(info) << "Attempting to connect to mempool...";
         while ( true )
         {
            KOINOS_TODO("Remove this loop when MQ client retry logic is implemented (koinos-mq-cpp#15)")
            pack::json j;
            pack::to_json( j, rpc::mempool::mempool_rpc_request{ rpc::mempool::mempool_reserved_request{} } );

            try
            {
               mq_client->rpc( mq::service::mempool, j.dump() ).get();
               LOG(info) << "Connected";
               break;
            }
            catch( const mq::timeout_error& ) {}
         }

         index( controller, mq_client );

         attach_client( controller, mq_client, amqp_url );
         attach_request_handler( controller, request_handler, amqp_url );
         LOG(info) << "Listening for requests over AMQP";
      }
      else
      {
         LOG(warning) << "Application is running without AMQP support";
      }

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
