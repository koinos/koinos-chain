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

#include <google/protobuf/util/json_util.h>

#include <koinos/chain/constants.hpp>
#include <koinos/chain/controller.hpp>
#include <koinos/chain/indexer.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/exception.hpp>
#include <koinos/mq/client.hpp>
#include <koinos/mq/request_handler.hpp>
#include <koinos/log.hpp>

#include <koinos/broadcast/broadcast.pb.h>
#include <koinos/rpc/block_store/block_store_rpc.pb.h>
#include <koinos/rpc/mempool/mempool_rpc.pb.h>

#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/options.hpp>
#include <koinos/util/random.hpp>
#include <koinos/util/services.hpp>

#define KOINOS_MAJOR_VERSION "0"
#define KOINOS_MINOR_VERSION "3"
#define KOINOS_PATCH_VERSION "0"

#define HELP_OPTION                         "help"
#define VERSION_OPTION                      "version"
#define BASEDIR_OPTION                      "basedir"
#define AMQP_OPTION                         "amqp"
#define AMQP_DEFAULT                        "amqp://guest:guest@localhost:5672/"
#define LOG_LEVEL_OPTION                    "log-level"
#define LOG_LEVEL_DEFAULT                   "info"
#define INSTANCE_ID_OPTION                  "instance-id"
#define STATEDIR_OPTION                     "statedir"
#define JOBS_OPTION                         "jobs"
#define JOBS_DEFAULT                        8
#define STATEDIR_DEFAULT                    "blockchain"
#define RESET_OPTION                        "reset"
#define GENESIS_DATA_FILE_OPTION            "genesis-data"
#define GENESIS_DATA_FILE_DEFAULT           "genesis_data.json"
#define READ_COMPUTE_BANDWITH_LIMIT_OPTION  "read-compute-bandwidth-limit"
#define READ_COMPUTE_BANDWITH_LIMIT_DEFAULT 10'000'000

KOINOS_DECLARE_EXCEPTION( service_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( invalid_argument, service_exception );

using namespace boost;
using namespace koinos;

const std::string& version_string();
void splash();
void attach_request_handler( chain::controller& controller, mq::request_handler& reqhandler );

int main( int argc, char** argv )
{
   std::atomic< bool > stopped = false;
   int retcode = EXIT_SUCCESS;

   try
   {
      program_options::options_description options;
      options.add_options()
         (HELP_OPTION                       ",h", "Print this help message and exit")
         (VERSION_OPTION                    ",v", "Print version string and exit")
         (BASEDIR_OPTION                    ",d", program_options::value< std::string >()->default_value( util::get_default_base_directory().string() ),
            "Koinos base directory")
         (AMQP_OPTION                       ",a", program_options::value< std::string >(), "AMQP server URL")
         (LOG_LEVEL_OPTION                  ",l", program_options::value< std::string >(), "The log filtering level")
         (INSTANCE_ID_OPTION                ",i", program_options::value< std::string >(), "An ID that uniquely identifies the instance")
         (JOBS_OPTION                       ",j", program_options::value< uint64_t    >(), "The number of worker jobs")
         (READ_COMPUTE_BANDWITH_LIMIT_OPTION",b", program_options::value< uint64_t    >(), "The compute bandwidth when reading contracts via the API")
         (GENESIS_DATA_FILE_OPTION          ",g", program_options::value< std::string >(), "The genesis data file")
         (STATEDIR_OPTION                       , program_options::value< std::string >(),
            "The location of the blockchain state files (absolute path or relative to basedir/chain)")
         (RESET_OPTION                          , program_options::value< bool >(), "Reset the database");

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
         chain_config = config[ util::service::chain ];
      }

      std::string amqp_url      = util::get_option< std::string >( AMQP_OPTION, AMQP_DEFAULT, args, chain_config, global_config );
      std::string log_level     = util::get_option< std::string >( LOG_LEVEL_OPTION, LOG_LEVEL_DEFAULT, args, chain_config, global_config );
      std::string instance_id   = util::get_option< std::string >( INSTANCE_ID_OPTION, util::random_alphanumeric( 5 ), args, chain_config, global_config );
      auto statedir             = std::filesystem::path( util::get_option< std::string >( STATEDIR_OPTION, STATEDIR_DEFAULT, args, chain_config, global_config ) );
      auto genesis_data_file    = std::filesystem::path( util::get_option< std::string >( GENESIS_DATA_FILE_OPTION, GENESIS_DATA_FILE_DEFAULT, args, chain_config, global_config ) );
      auto reset                = util::get_option< bool >( RESET_OPTION, false, args, chain_config, global_config );
      auto jobs                 = util::get_option< uint64_t >( JOBS_OPTION, JOBS_DEFAULT, args, chain_config, global_config );
      auto read_compute_limit   = util::get_option< uint64_t >( READ_COMPUTE_BANDWITH_LIMIT_OPTION, READ_COMPUTE_BANDWITH_LIMIT_DEFAULT, args, chain_config, global_config );

      koinos::initialize_logging( util::service::chain, instance_id, log_level, basedir / util::service::chain / "logs" );

      KOINOS_ASSERT( jobs > 0, invalid_argument, "jobs must be greater than 0" );

      if ( config.IsNull() )
      {
         LOG(warning) << "Could not find config (config.yml or config.yaml expected). Using default values";
      }

      if ( statedir.is_relative() )
         statedir = basedir / util::service::chain / statedir;

      if ( !std::filesystem::exists( statedir ) )
         std::filesystem::create_directories( statedir );

      // Load genesis data
      if ( genesis_data_file.is_relative() )
         genesis_data_file = basedir / util::service::chain / genesis_data_file;

      KOINOS_ASSERT(
         std::filesystem::exists( genesis_data_file ),
         invalid_argument,
         "unable to locate genesis data file at ${loc}", ("loc", genesis_data_file.string())
      );

      std::ifstream gifs( genesis_data_file );
      std::stringstream genesis_data_stream;
      genesis_data_stream << gifs.rdbuf();
      std::string genesis_json = genesis_data_stream.str();
      gifs.close();

      chain::genesis_data genesis_data;
      google::protobuf::util::JsonParseOptions jpo;
      google::protobuf::util::JsonStringToMessage( genesis_json, &genesis_data, jpo );

      crypto::multihash chain_id = crypto::hash( crypto::multicodec::sha2_256, genesis_data );

      LOG(info) << "Chain ID: " << chain_id;
      LOG(info) << "Number of jobs: " << jobs;

      asio::io_context client_ioc, server_ioc, main_ioc;
      auto client = std::make_shared< mq::client >( client_ioc );
      auto request_handler = mq::request_handler( server_ioc );

      asio::signal_set signals( main_ioc );
      signals.add( SIGINT );
      signals.add( SIGTERM );
#if defined( SIGQUIT )
      signals.add( SIGQUIT );
#endif

      signals.async_wait( [&]( const system::error_code& err, int num )
      {
         LOG(info) << "Caught signal, shutting down...";
         stopped = true;
         main_ioc.stop();
         client_ioc.stop();
         server_ioc.stop();
      } );

      std::vector< std::thread > client_threads;
      for ( std::size_t i = 0; i < jobs; i++ )
         client_threads.emplace_back( [&]() { client_ioc.run(); } );

      std::vector< std::thread > server_threads;
      for ( std::size_t i = 0; i < jobs; i++ )
         server_threads.emplace_back( [&]() { server_ioc.run(); } );

      chain::controller controller( read_compute_limit );
      controller.open( statedir, genesis_data, reset );

      LOG(info) << "Connecting AMQP client...";
      client->connect( amqp_url );
      LOG(info) << "Established AMQP client connection to the server";

      LOG(info) << "Attempting to connect to block_store...";
      rpc::block_store::block_store_request b_req;
      b_req.mutable_reserved();
      client->rpc( util::service::block_store, b_req.SerializeAsString() ).get();
      LOG(info) << "Established connection to block_store";

      LOG(info) << "Attempting to connect to mempool...";
      rpc::mempool::mempool_request m_req;
      m_req.mutable_reserved();
      client->rpc( util::service::mempool, m_req.SerializeAsString() ).get();
      LOG(info) << "Established connection to mempool";

      chain::indexer indexer( client_ioc, controller, client );

      if ( indexer.index().get() )
      {
         controller.set_client( client );
         attach_request_handler( controller, request_handler );

         LOG(info) << "Connecting AMQP request handler...";
         request_handler.connect( amqp_url );
         LOG(info) << "Established request handler connection to the AMQP server";

         LOG(info) << "Listening for requests over AMQP";
         auto work = asio::make_work_guard( main_ioc );
         main_ioc.run();
      }

      for ( auto& t : client_threads )
         t.join();

      for ( auto& t : server_threads )
         t.join();
   }
   catch ( const invalid_argument& e )
   {
      LOG(error) << "Invalid argument: " << e.what();
      retcode = EXIT_FAILURE;
   }
   catch ( const koinos::exception& e )
   {
      if ( !stopped )
      {
         LOG(fatal) << "An unexpected error has occurred: " << e.what();
         retcode = EXIT_FAILURE;
      }
   }
   catch ( const boost::exception& e )
   {
      LOG(fatal) << "An unexpected error has occurred: " << boost::diagnostic_information( e );
      retcode = EXIT_FAILURE;
   }
   catch ( const std::exception& e )
   {
      LOG(fatal) << "An unexpected error has occurred: " << e.what();
      retcode = EXIT_FAILURE;
   }
   catch ( ... )
   {
      LOG(fatal) << "An unexpected error has occurred";
      retcode = EXIT_FAILURE;
   }

   LOG(info) << "Shut down gracefully";

   return retcode;
}

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

void attach_request_handler(
   chain::controller& controller,
   mq::request_handler& reqhandler )
{
   reqhandler.add_rpc_handler(
      util::service::chain,
      [&]( const std::string& msg ) -> std::string
      {
         rpc::chain::chain_request args;
         rpc::chain::chain_response resp;

         if ( args.ParseFromString( msg ) )
         {
            LOG(debug) << "Received RPC: " << args;

            try
            {
               switch( args.request_case() )
               {
                  case rpc::chain::chain_request::RequestCase::kReserved:
                     resp.mutable_reserved();
                     break;
                  case rpc::chain::chain_request::RequestCase::kSubmitBlock:
                     *resp.mutable_submit_block() = controller.submit_block( args.submit_block() );
                     break;
                  case rpc::chain::chain_request::RequestCase::kSubmitTransaction:
                     *resp.mutable_submit_transaction() = controller.submit_transaction( args.submit_transaction() );
                     break;
                  case rpc::chain::chain_request::RequestCase::kGetHeadInfo:
                     *resp.mutable_get_head_info() = controller.get_head_info( args.get_head_info() );
                     break;
                  case rpc::chain::chain_request::RequestCase::kGetChainId:
                     *resp.mutable_get_chain_id() = controller.get_chain_id( args.get_chain_id() );
                     break;
                  case rpc::chain::chain_request::RequestCase::kGetForkHeads:
                     *resp.mutable_get_fork_heads() = controller.get_fork_heads( args.get_fork_heads() );
                     break;
                  case rpc::chain::chain_request::RequestCase::kReadContract:
                     *resp.mutable_read_contract() = controller.read_contract( args.read_contract() );
                     break;
                  case rpc::chain::chain_request::RequestCase::kGetAccountNonce:
                     *resp.mutable_get_account_nonce() = controller.get_account_nonce( args.get_account_nonce() );
                     break;
                  case rpc::chain::chain_request::RequestCase::kGetAccountRc:
                     *resp.mutable_get_account_rc() = controller.get_account_rc( args.get_account_rc() );
                     break;
                  case rpc::chain::chain_request::RequestCase::kGetResourceLimits:
                     *resp.mutable_get_resource_limits() = controller.get_resource_limits( args.get_resource_limits() );
                     break;
                  default:
                     resp.mutable_error()->set_message( "Error: attempted to call unknown rpc" );
                     break;
               }
            }
            catch( const koinos::exception& e )
            {
               auto error = resp.mutable_error();
               error->set_message( e.what() );
               error->set_data( e.get_json().dump() );
            }
            catch( std::exception& e )
            {
               resp.mutable_error()->set_message( e.what() );
            }
            catch( ... )
            {
               LOG(error) << "Unexpected error while handling rpc: " << args.ShortDebugString();
               resp.mutable_error()->set_message( "Unexpected error while handling rpc" );
            }
         }
         else
         {
            LOG(warning) << "Received bad message";
            resp.mutable_error()->set_message( "Received bad message" );
         }

         LOG(debug) << "Sending RPC response: " << resp;

         std::string r;
         resp.SerializeToString( &r );
         return r;
      }
   );

   reqhandler.add_broadcast_handler(
      "koinos.block.accept",
      [&]( const std::string& msg )
      {
         broadcast::block_accepted bam;
         if ( !bam.ParseFromString( msg ) )
         {
            LOG(warning) << "Could not parse block accepted broadcast";
            return;
         }

         try
         {
            rpc::chain::submit_block_request sub_block;
            sub_block.set_allocated_block( bam.release_block() );
            controller.submit_block( sub_block );
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
}
