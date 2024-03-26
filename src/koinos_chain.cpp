#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <boost/thread/sync_bounded_queue.hpp>

#include <yaml-cpp/yaml.h>

#include <google/protobuf/util/json_util.h>

#include <koinos/chain/constants.hpp>
#include <koinos/chain/controller.hpp>
#include <koinos/chain/indexer.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/exception.hpp>
#include <koinos/log.hpp>
#include <koinos/mq/client.hpp>
#include <koinos/mq/request_handler.hpp>

#include <koinos/broadcast/broadcast.pb.h>
#include <koinos/rpc/block_store/block_store_rpc.pb.h>
#include <koinos/rpc/mempool/mempool_rpc.pb.h>

#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/options.hpp>
#include <koinos/util/random.hpp>
#include <koinos/util/services.hpp>

#include "git_version.h"

#define FIFO_ALGORITHM       "fifo"
#define BLOCK_TIME_ALGORITHM "block-time"
#define POB_ALGORITHM        "pob"

#define HELP_OPTION                         "help"
#define VERSION_OPTION                      "version"
#define BASEDIR_OPTION                      "basedir"
#define AMQP_OPTION                         "amqp"
#define AMQP_DEFAULT                        "amqp://guest:guest@localhost:5672/"
#define LOG_LEVEL_OPTION                    "log-level"
#define LOG_LEVEL_DEFAULT                   "info"
#define LOG_DIR_OPTION                      "log-dir"
#define LOG_DIR_DEFAULT                     ""
#define LOG_COLOR_OPTION                    "log-color"
#define LOG_COLOR_DEFAULT                   true
#define LOG_DATETIME_OPTION                 "log-datetime"
#define LOG_DATETIME_DEFAULT                true
#define INSTANCE_ID_OPTION                  "instance-id"
#define STATEDIR_OPTION                     "statedir"
#define JOBS_OPTION                         "jobs"
#define JOBS_DEFAULT                        uint64_t( 2 )
#define STATEDIR_DEFAULT                    "blockchain"
#define RESET_OPTION                        "reset"
#define GENESIS_DATA_FILE_OPTION            "genesis-data"
#define GENESIS_DATA_FILE_DEFAULT           "genesis_data.json"
#define READ_COMPUTE_BANDWITH_LIMIT_OPTION  "read-compute-bandwidth-limit"
#define READ_COMPUTE_BANDWITH_LIMIT_DEFAULT 10'000'000
#define SYSTEM_CALL_BUFFER_SIZE_OPTION      "system-call-buffer-size"
#define SYSTEM_CALL_BUFFER_SIZE_DEFAULT     64'000
#define FORK_ALGORITHM_OPTION               "fork-algorithm"
#define FORK_ALGORITHM_DEFAULT              FIFO_ALGORITHM

KOINOS_DECLARE_EXCEPTION( service_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( invalid_argument, service_exception );

using namespace boost;
using namespace koinos;

const std::string& version_string();
void attach_request_handler( chain::controller& controller, mq::request_handler& reqhandler );

int main( int argc, char** argv )
{
  std::string amqp_url, log_level, log_dir, instance_id, fork_algorithm_option;
  std::filesystem::path statedir, genesis_data_file;
  uint64_t jobs, read_compute_limit;
  int32_t syscall_bufsize;
  chain::genesis_data genesis_data;
  bool reset, log_color, log_datetime;
  chain::fork_resolution_algorithm fork_algorithm;

  try
  {
    program_options::options_description options;

    // clang-format off
    options.add_options()
      ( HELP_OPTION ",h"                       , "Print this help message and exit" )
      ( VERSION_OPTION ",v"                    , "Print version string and exit" )
      ( BASEDIR_OPTION ",d"                    , program_options::value< std::string >()->default_value( util::get_default_base_directory().string() ), "Koinos base directory" )
      ( AMQP_OPTION ",a"                       , program_options::value< std::string >(), "AMQP server URL" )
      ( LOG_LEVEL_OPTION ",l"                  , program_options::value< std::string >(), "The log filtering level" )
      ( INSTANCE_ID_OPTION ",i"                , program_options::value< std::string >(), "An ID that uniquely identifies the instance" )
      ( JOBS_OPTION ",j"                       , program_options::value< uint64_t >()   , "The number of worker jobs" )
      ( READ_COMPUTE_BANDWITH_LIMIT_OPTION ",b", program_options::value< uint64_t >()   , "The compute bandwidth when reading contracts via the API" )
      ( GENESIS_DATA_FILE_OPTION ",g"          , program_options::value< std::string >(), "The genesis data file" )
      ( STATEDIR_OPTION                        , program_options::value< std::string >(), "The location of the blockchain state files (absolute path or relative to basedir/chain)" )
      ( RESET_OPTION                           , program_options::value< bool >()       , "Reset the database" )
      ( FORK_ALGORITHM_OPTION ",f"             , program_options::value< std::string >(), "The fork resolution algorithm to use. Can be 'fifo', 'pob', or 'block-time'. (Default: 'fifo')" )
      ( LOG_DIR_OPTION                         , program_options::value< std::string >(), "The logging directory" )
      ( LOG_COLOR_OPTION                       , program_options::value< bool >()       , "Log color toggle" )
      ( LOG_DATETIME_OPTION                    , program_options::value< bool >()       , "Log datetime on console toggle" )
      ( SYSTEM_CALL_BUFFER_SIZE_OPTION         , program_options::value< uint32_t >()   , "System call RPC invocation buffer size" );
    // clang-format on

    program_options::variables_map args;
    program_options::store( program_options::parse_command_line( argc, argv, options ), args );

    if( args.count( HELP_OPTION ) )
    {
      std::cout << options << std::endl;
      return EXIT_SUCCESS;
    }

    if( args.count( VERSION_OPTION ) )
    {
      const auto& v_str = version_string();
      std::cout.write( v_str.c_str(), v_str.size() );
      std::cout << std::endl;
      return EXIT_SUCCESS;
    }

    auto basedir = std::filesystem::path( args[ BASEDIR_OPTION ].as< std::string >() );
    if( basedir.is_relative() )
      basedir = std::filesystem::current_path() / basedir;

    YAML::Node config;
    YAML::Node global_config;
    YAML::Node chain_config;

    auto yaml_config = basedir / "config.yml";
    if( !std::filesystem::exists( yaml_config ) )
    {
      yaml_config = basedir / "config.yaml";
    }

    if( std::filesystem::exists( yaml_config ) )
    {
      config        = YAML::LoadFile( yaml_config );
      global_config = config[ "global" ];
      chain_config  = config[ util::service::chain ];
    }

    // clang-format off
    amqp_url              = util::get_option< std::string >( AMQP_OPTION, AMQP_DEFAULT, args, chain_config, global_config );
    log_level             = util::get_option< std::string >( LOG_LEVEL_OPTION, LOG_LEVEL_DEFAULT, args, chain_config, global_config );
    log_dir               = util::get_option< std::string >( LOG_DIR_OPTION, LOG_DIR_DEFAULT, args, chain_config, global_config );
    log_color             = util::get_option< bool >( LOG_COLOR_OPTION, LOG_COLOR_DEFAULT, args, chain_config, global_config );
    log_datetime          = util::get_option< bool >( LOG_DATETIME_OPTION, LOG_DATETIME_DEFAULT, args, chain_config, global_config );
    instance_id           = util::get_option< std::string >( INSTANCE_ID_OPTION, util::random_alphanumeric( 5 ), args, chain_config, global_config );
    statedir              = std::filesystem::path( util::get_option< std::string >( STATEDIR_OPTION, STATEDIR_DEFAULT, args, chain_config, global_config ) );
    genesis_data_file     = std::filesystem::path( util::get_option< std::string >( GENESIS_DATA_FILE_OPTION, GENESIS_DATA_FILE_DEFAULT, args, chain_config, global_config ) );
    reset                 = util::get_option< bool >( RESET_OPTION, false, args, chain_config, global_config );
    jobs                  = util::get_option< uint64_t >( JOBS_OPTION, std::max( JOBS_DEFAULT, uint64_t( std::thread::hardware_concurrency() ) ), args, chain_config, global_config );
    read_compute_limit    = util::get_option< uint64_t >( READ_COMPUTE_BANDWITH_LIMIT_OPTION, READ_COMPUTE_BANDWITH_LIMIT_DEFAULT, args, chain_config, global_config );
    fork_algorithm_option = util::get_option< std::string >( FORK_ALGORITHM_OPTION, FORK_ALGORITHM_DEFAULT, args, chain_config, global_config );
    syscall_bufsize       = util::get_option< uint32_t >( SYSTEM_CALL_BUFFER_SIZE_OPTION, SYSTEM_CALL_BUFFER_SIZE_DEFAULT, args, chain_config, global_config );
    // clang-format on

    std::optional< std::filesystem::path > logdir_path;
    if( !log_dir.empty() )
    {
      logdir_path = std::make_optional< std::filesystem::path >( log_dir );
      if( logdir_path->is_relative() )
        logdir_path = basedir / util::service::chain / *logdir_path;
    }

    koinos::initialize_logging( util::service::chain, instance_id, log_level, logdir_path, log_color, log_datetime );

    LOG( info ) << version_string();

    KOINOS_ASSERT( jobs > 1, invalid_argument, "jobs must be greater than 1" );

    if( config.IsNull() )
    {
      LOG( warning ) << "Could not find config (config.yml or config.yaml expected). Using default values";
    }

    if( fork_algorithm_option == FIFO_ALGORITHM )
    {
      LOG( info ) << "Using fork resolution algorithm: " << FIFO_ALGORITHM;
      fork_algorithm = chain::fork_resolution_algorithm::fifo;
    }
    else if( fork_algorithm_option == BLOCK_TIME_ALGORITHM )
    {
      LOG( info ) << "Using fork resolution algorithm: " << BLOCK_TIME_ALGORITHM;
      fork_algorithm = chain::fork_resolution_algorithm::block_time;
    }
    else if( fork_algorithm_option == POB_ALGORITHM )
    {
      LOG( info ) << "Using fork resolution algorithm: " << POB_ALGORITHM;
      fork_algorithm = chain::fork_resolution_algorithm::pob;
    }
    else
    {
      KOINOS_THROW( invalid_argument, "${a} is not a valid fork algorithm", ( "a", fork_algorithm_option ) );
    }

    if( statedir.is_relative() )
      statedir = basedir / util::service::chain / statedir;

    if( !std::filesystem::exists( statedir ) )
      std::filesystem::create_directories( statedir );

    // Load genesis data
    if( genesis_data_file.is_relative() )
      genesis_data_file = basedir / util::service::chain / genesis_data_file;

    KOINOS_ASSERT( std::filesystem::exists( genesis_data_file ),
                   invalid_argument,
                   "unable to locate genesis data file at ${loc}",
                   ( "loc", genesis_data_file.string() ) );

    std::ifstream gifs( genesis_data_file );
    std::stringstream genesis_data_stream;
    genesis_data_stream << gifs.rdbuf();
    std::string genesis_json = genesis_data_stream.str();
    gifs.close();

    google::protobuf::util::JsonParseOptions jpo;
    google::protobuf::util::JsonStringToMessage( genesis_json, &genesis_data, jpo );

    crypto::multihash chain_id = crypto::hash( crypto::multicodec::sha2_256, genesis_data );

    LOG( info ) << "Chain ID: " << chain_id;
    LOG( info ) << "Number of jobs: " << jobs;
  }
  catch( const invalid_argument& e )
  {
    LOG( error ) << "Invalid argument: " << e.what();
    return EXIT_FAILURE;
  }

  std::atomic< bool > stopped = false;
  int retcode                 = EXIT_SUCCESS;
  std::vector< boost::thread > threads;

  asio::io_context client_ioc, server_ioc, main_ioc;
  auto client          = std::make_shared< mq::client >( client_ioc );
  auto request_handler = mq::request_handler( server_ioc );
  chain::controller controller( read_compute_limit, syscall_bufsize );

  try
  {
    asio::signal_set signals( server_ioc );
    signals.add( SIGINT );
    signals.add( SIGTERM );
#if defined( SIGQUIT )
    signals.add( SIGQUIT );
#endif

    signals.async_wait(
      [ & ]( const system::error_code& err, int num )
      {
        LOG( info ) << "Caught signal, shutting down...";
        stopped = true;
        main_ioc.stop();
      } );

    boost::thread::attributes attrs;
    attrs.set_stack_size( 8'192 * 1'024 );

    threads.emplace_back( attrs,
                          [ & ]()
                          {
                            client_ioc.run();
                          } );
    threads.emplace_back( attrs,
                          [ & ]()
                          {
                            client_ioc.run();
                          } );
    for( std::size_t i = 0; i < jobs; i++ )
      threads.emplace_back( attrs,
                            [ & ]()
                            {
                              server_ioc.run();
                            } );

    controller.open( statedir, genesis_data, fork_algorithm, reset );

    LOG( info ) << "Connecting AMQP client...";
    client->connect( amqp_url );
    LOG( info ) << "Established AMQP client connection to the server";

    LOG( info ) << "Attempting to connect to block_store...";
    rpc::block_store::block_store_request b_req;
    b_req.mutable_reserved();
    client->rpc( util::service::block_store, b_req.SerializeAsString() ).get();
    LOG( info ) << "Established connection to block_store";

    LOG( info ) << "Attempting to connect to mempool...";
    rpc::mempool::mempool_request m_req;
    m_req.mutable_reserved();
    client->rpc( util::service::mempool, m_req.SerializeAsString() ).get();
    LOG( info ) << "Established connection to mempool";

    chain::indexer indexer( client_ioc, controller, client );

    if( indexer.index().get() )
    {
      controller.set_client( client );
      attach_request_handler( controller, request_handler );

      LOG( info ) << "Connecting AMQP request handler...";
      request_handler.connect( amqp_url );
      LOG( info ) << "Established request handler connection to the AMQP server";

      LOG( info ) << "Listening for requests over AMQP";
      auto work = asio::make_work_guard( main_ioc );
      main_ioc.run();
    }
  }
  catch( const std::exception& e )
  {
    if( !stopped )
    {
      LOG( fatal ) << "An unexpected error has occurred: " << e.what();
      retcode = EXIT_FAILURE;
    }
  }
  catch( const boost::exception& e )
  {
    LOG( fatal ) << "An unexpected error has occurred: " << boost::diagnostic_information( e );
    retcode = EXIT_FAILURE;
  }
  catch( ... )
  {
    LOG( fatal ) << "An unexpected error has occurred";
    retcode = EXIT_FAILURE;
  }

  controller.close();

  for( auto& t: threads )
    t.join();

  LOG( info ) << "Shut down gracefully";

  return retcode;
}

const std::string& version_string()
{
  static std::string v_str = "Koinos Chain v";
  v_str += std::to_string( KOINOS_MAJOR_VERSION ) + "." + std::to_string( KOINOS_MINOR_VERSION ) + "."
           + std::to_string( KOINOS_PATCH_VERSION );
  v_str += " (" + std::string( KOINOS_GIT_HASH ) + ")";
  return v_str;
}

void attach_request_handler( chain::controller& controller, mq::request_handler& reqhandler )
{
  reqhandler.add_rpc_handler(
    util::service::chain,
    [ & ]( const std::string& msg ) -> std::string
    {
      rpc::chain::chain_request args;
      rpc::chain::chain_response resp;

      if( args.ParseFromString( msg ) )
      {
        LOG( debug ) << "Received RPC: " << args;

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
            case rpc::chain::chain_request::RequestCase::kInvokeSystemCall:
              *resp.mutable_invoke_system_call() = controller.invoke_system_call( args.invoke_system_call() );
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

          auto j      = e.get_json();
          j[ "code" ] = e.get_code();
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
          error->set_data( j.dump() );
#pragma clang diagnostic pop

          chain::error_details details;
          details.set_code( e.get_code() );

          if( const auto& logs = j[ "logs" ]; logs.is_array() )
            for( const auto& line: logs )
              details.add_logs( line.get< std::string >() );

          error->add_details()->PackFrom( details );
        }
        catch( std::exception& e )
        {
          auto error = resp.mutable_error();
          error->set_message( e.what() );

          nlohmann::json j;
          j[ "code" ] = chain::internal_error;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
          error->set_data( j.dump() );
#pragma clang diagnostic pop

          chain::error_details details;
          details.set_code( chain::internal_error );
          error->add_details()->PackFrom( details );
        }
        catch( ... )
        {
          LOG( error ) << "Unexpected error while handling rpc: " << args.ShortDebugString();

          auto error = resp.mutable_error();
          error->set_message( "unexpected error while handling rpc" );

          nlohmann::json j;
          j[ "code" ] = chain::internal_error;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
          error->set_data( j.dump() );
#pragma clang diagnostic pop

          chain::error_details details;
          details.set_code( chain::internal_error );
          error->add_details()->PackFrom( details );
        }
      }
      else
      {
        LOG( warning ) << "Received bad message";

        auto error = resp.mutable_error();
        error->set_message( "received bad message" );

        nlohmann::json j;
        j[ "code" ] = chain::internal_error;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        error->set_data( j.dump() );
#pragma clang diagnostic pop

        chain::error_details details;
        details.set_code( chain::internal_error );
        error->add_details()->PackFrom( details );
      }

      LOG( debug ) << "Sending RPC response: " << resp;

      std::string r;
      resp.SerializeToString( &r );
      return r;
    } );

  reqhandler.add_broadcast_handler( "koinos.block.accept",
                                    [ & ]( const std::string& msg )
                                    {
                                      broadcast::block_accepted bam;
                                      if( !bam.ParseFromString( msg ) )
                                      {
                                        LOG( warning ) << "Could not parse block accepted broadcast";
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
                                        LOG( warning )
                                          << "Error handling block broadcast: " << boost::diagnostic_information( e );
                                      }
                                      catch( const std::exception& e )
                                      {
                                        LOG( warning ) << "Error handling block broadcast: " << e.what();
                                      }
                                    } );
}
