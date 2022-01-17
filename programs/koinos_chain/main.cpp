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
#define KOINOS_MINOR_VERSION "1"
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
#define STATEDIR_DEFAULT                    "blockchain"
#define RESET_OPTION                        "reset"
#define GENESIS_DATA_FILE_OPTION            "genesis-data"
#define GENESIS_DATA_FILE_DEFAULT           "genesis_data.json"
#define READ_COMPUTE_BANDWITH_LIMIT_OPTION  "read-compute-bandwidth-limit"
#define READ_COMPUTE_BANDWITH_LIMIT_DEFAULT 10'000'000

using namespace boost;
using namespace koinos;

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
   mq::request_handler& mq_reqhandler,
   const std::string& amqp_url )
{
   mq_reqhandler.add_rpc_handler(
      util::service::chain,
      [&]( const std::string& msg ) -> std::string
      {
         rpc::chain::chain_request args;
         rpc::chain::chain_response resp;

         if ( args.ParseFromString( msg ) )
         {
            LOG(debug) << "Received rpc: " << args;

            try
            {
               switch( args.request_case() )
               {
                  case rpc::chain::chain_request::RequestCase::kReserved:
                  {
                     resp.mutable_reserved();
                     break;
                  }
                  case rpc::chain::chain_request::RequestCase::kSubmitBlock:
                  {
                     *resp.mutable_submit_block() = controller.submit_block( args.submit_block() );
                     break;
                  }
                  case rpc::chain::chain_request::RequestCase::kSubmitTransaction:
                  {
                     *resp.mutable_submit_transaction() = controller.submit_transaction( args.submit_transaction() );
                     break;
                  }
                  case rpc::chain::chain_request::RequestCase::kGetHeadInfo:
                  {
                     *resp.mutable_get_head_info() = controller.get_head_info( args.get_head_info() );
                     break;
                  }
                  case rpc::chain::chain_request::RequestCase::kGetChainId:
                  {
                     *resp.mutable_get_chain_id() = controller.get_chain_id( args.get_chain_id() );
                     break;
                  }
                  case rpc::chain::chain_request::RequestCase::kGetForkHeads:
                  {
                     *resp.mutable_get_fork_heads() = controller.get_fork_heads( args.get_fork_heads() );
                     break;
                  }
                  case rpc::chain::chain_request::RequestCase::kReadContract:
                  {
                     *resp.mutable_read_contract() = controller.read_contract( args.read_contract() );
                     break;
                  }
                  case rpc::chain::chain_request::RequestCase::kGetAccountNonce:
                  {
                     *resp.mutable_get_account_nonce() = controller.get_account_nonce( args.get_account_nonce() );
                     break;
                  }
                  case rpc::chain::chain_request::RequestCase::kGetAccountRc:
                  {
                     *resp.mutable_get_account_rc() = controller.get_account_rc( args.get_account_rc() );
                     break;
                  }
                  case rpc::chain::chain_request::RequestCase::kGetResourceLimits:
                  {
                     *resp.mutable_get_resource_limits() = controller.get_resource_limits( args.get_resource_limits() );
                     break;
                  }
                  default:
                  {
                     resp.mutable_error()->set_message( "Error: attempted to call unknown rpc" );
                  }
               }
            }
            catch( const koinos::exception& e )
            {
               auto error = resp.mutable_error();
               error->set_message( e.what() );
               error->set_data( e.get_json() );
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

         LOG(debug) << "Sending rpc response: " << resp;

         std::string r;
         resp.SerializeToString( &r );
         return r;
      }
   );

   mq_reqhandler.add_broadcast_handler(
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

   LOG(info) << "Connecting AMQP request handler...";
   mq_reqhandler.connect( amqp_url );
   LOG(info) << "Established request handler connection to the AMQP server";
}


void index_loop(
   chain::controller& controller,
   concurrent::sync_bounded_queue< std::shared_future< std::string > >& rpc_queue,
   uint64_t last_height )
{
   while ( true )
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
         rpc::block_store::get_blocks_by_height_response* batch = nullptr;

         if ( resp.ParseFromString( future.get() ) )
         {
            switch( resp.response_case() )
            {
               case rpc::block_store::block_store_response::ResponseCase::kGetBlocksByHeight:
               {
                  batch = resp.mutable_get_blocks_by_height();
                  break;
               }
               case rpc::block_store::block_store_response::ResponseCase::kError:
               {
                  KOINOS_THROW( koinos::exception, resp.error().message() );
                  break;
               }
               default:
               {
                  KOINOS_THROW( koinos::exception, "unexpected block store response" );
                  break;
               }
            }
         }

         rpc::chain::submit_block_request sub_block;

         for ( auto& block_item : *batch->mutable_block_items() )
         {
            sub_block.set_allocated_block( block_item.release_block() );
            controller.submit_block( sub_block, last_height );
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
      rpc::block_store::block_store_request req;
      req.mutable_get_highest_block();
      std::string req_str;
      req.SerializeToString( &req_str );
      auto future = mq_client->rpc( util::service::block_store, req_str );

      rpc::block_store::block_store_response resp;
      block_topology target_head;

      if( resp.ParseFromString( future.get() ) )
      {
         switch( resp.response_case() )
            {
               case rpc::block_store::block_store_response::ResponseCase::kGetHighestBlock:
               {
                  target_head.CopyFrom( resp.get_highest_block().topology() );
                  break;
               }
               case rpc::block_store::block_store_response::ResponseCase::kError:
               {
                  KOINOS_THROW( koinos::exception, resp.error().message() );
                  break;
               }
               default:
               {
                  KOINOS_THROW( koinos::exception, "unexpected block store response" );
                  break;
               }
            }
      }
      else
      {
         LOG(error) << "Could not get highest block from block store";
         exit( EXIT_FAILURE );
      }

      auto head_info = controller.get_head_info();

      if ( head_info.head_topology().height() < target_head.height() )
      {
         LOG(info) << "Indexing to target block: " << target_head;

         concurrent::sync_bounded_queue< std::shared_future< std::string > > rpc_queue{ 10 };

         auto index_thread = std::make_unique< std::thread >( [&]()
         {
            index_loop( controller, rpc_queue, target_head.height() );
         } );

         crypto::multihash last_id = crypto::multihash::zero( crypto::multicodec::sha2_256 );
         uint64_t last_height = head_info.head_topology().height();

         while ( last_height < target_head.height() )
         {
            auto* by_height_req = req.mutable_get_blocks_by_height();
            by_height_req->set_head_block_id( target_head.id() );
            by_height_req->set_ancestor_start_height( last_height + 1 );
            by_height_req->set_num_blocks( batch_size );
            by_height_req->set_return_block( true );
            by_height_req->set_return_receipt( false );

            req.SerializeToString( &req_str );
            rpc_queue.push_back( mq_client->rpc( util::service::block_store, req_str ) );
            last_height += batch_size;
         }

         rpc_queue.close();
         index_thread->join();

         auto new_head_info = controller.get_head_info();

         const std::chrono::duration< double > duration = std::chrono::system_clock::now() - before;
         LOG(info) << "Finished indexing " << new_head_info.head_topology().height() - head_info.head_topology().height() << " blocks, took " << duration.count() << " seconds";
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
         (RESET_OPTION                          , program_options::bool_switch()->default_value(false), "Reset the database");

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
      auto reset                = util::get_flag( RESET_OPTION, false, args, chain_config, global_config );
      auto jobs                 = util::get_option< uint64_t >( JOBS_OPTION, std::thread::hardware_concurrency(), args, chain_config, global_config );
      auto read_compute_limit   = util::get_option< uint64_t >( READ_COMPUTE_BANDWITH_LIMIT_OPTION, READ_COMPUTE_BANDWITH_LIMIT_DEFAULT, args, chain_config, global_config );

      koinos::initialize_logging( util::service::chain, instance_id, log_level, basedir / util::service::chain );

      KOINOS_ASSERT( jobs > 0, koinos::exception, "jobs must be greater than 0" );

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
         koinos::exception,
         "unable to locate genesis data file at: ${loc}", ("loc", genesis_data_file.string())
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

      chain::controller controller( read_compute_limit );
      controller.open( statedir, genesis_data, reset );

      asio::io_context main_context, work_context;
      auto mq_client = std::make_shared< mq::client >();
      auto request_handler = mq::request_handler( work_context );

      LOG(info) << "Connecting AMQP client...";
      mq_client->connect( amqp_url );
      LOG(info) << "Established AMQP client connection to the server";

      std::string result;

      LOG(info) << "Attempting to connect to block_store...";
      rpc::block_store::block_store_request b_req;
      b_req.mutable_reserved();
      b_req.SerializeToString( &result );
      mq_client->rpc( util::service::block_store, result ).get();
      LOG(info) << "Established connection to block_store";

      result.clear();

      LOG(info) << "Attempting to connect to mempool...";
      rpc::mempool::mempool_request m_req;
      m_req.mutable_reserved();
      m_req.SerializeToString( &result );
      mq_client->rpc( util::service::mempool, result ).get();
      LOG(info) << "Established connection to mempool";

      index( controller, mq_client );
      controller.set_client( mq_client );

      attach_request_handler( controller, request_handler, amqp_url );
      LOG(info) << "Listening for requests over AMQP";

      asio::signal_set signals( main_context, SIGINT, SIGTERM );

      signals.async_wait( [&]( const system::error_code& err, int num )
      {
         LOG(info) << "Caught signal, shutting down...";
         boost::asio::post(
            main_context,
            [&]()
            {
               work_context.stop();
               main_context.stop();
               mq_client->disconnect();
            }
         );
      } );

      std::vector< std::thread > threads;
      for ( std::size_t i = 0; i < jobs; i++ )
         threads.emplace_back( [&]() { work_context.run(); } );

      main_context.run();

      for ( auto& t : threads )
         t.join();

      LOG(info) << "Shut down successfully";

      return EXIT_SUCCESS;
   }
   catch ( const boost::exception& e )
   {
      LOG(fatal) << boost::diagnostic_information( e );
   }
   catch ( const std::exception& e )
   {
      LOG(fatal) << e.what();
   }
   catch ( ... )
   {
      LOG(fatal) << "Unknown exception";
   }

   return EXIT_FAILURE;
}
