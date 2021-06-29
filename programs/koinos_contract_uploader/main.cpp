#include <iostream>
#include <fstream>
#include <filesystem>

#include <boost/program_options.hpp>

#include <koinos/exception.hpp>
#include <koinos/log.hpp>
#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/pack/classes.hpp>
#include <koinos/mq/client.hpp>
#include <koinos/util.hpp>

#define HELP_OPTION               "help"
#define PRIVATE_KEY_FILE_OPTION   "private-key-file"
#define PRIVATE_KEY_FILE_DEFAULT  "private.key"
#define AMQP_OPTION               "amqp"
#define AMQP_DEFAULT              "amqp://guest:guest@localhost:5672/"
#define CONTRACT_OPTION           "contract"
#define CALL_ID_OPTION            "call-id"
#define ENTRY_POINT_OPTION        "entry-point"
#define CONTRACT_ID_OPTION        "contract-id"
#define UPLOAD_OPTION             "upload"
#define OVERRIDE_OPTION           "override"

using namespace boost;
using namespace koinos;

uint64_t get_next_nonce( std::shared_ptr< mq::client > client, const std::string& account );
void submit_transaction( std::shared_ptr< mq::client > client, const protocol::transaction& t );

int main( int argc, char** argv )
{
   try
   {
      program_options::options_description options( "Options" );
      options.add_options()
         (HELP_OPTION              ",h", "Print usage message")
         (AMQP_OPTION              ",a", program_options::value< std::string >()->default_value( AMQP_DEFAULT ), "AMQP server URL")
         (PRIVATE_KEY_FILE_OPTION  ",p", program_options::value< std::string >()->default_value( PRIVATE_KEY_FILE_DEFAULT ), "The private key file")

         // --upload arguments
         (UPLOAD_OPTION                , "Run in upload mode")
         (CONTRACT_OPTION          ",c", program_options::value< std::string >(), "The wasm contract")

         // --override arguments
         (OVERRIDE_OPTION              , "Run in override mode")
         (CALL_ID_OPTION           ",o", program_options::value< uint32_t >()   , "The system call ID to override")
         (ENTRY_POINT_OPTION       ",e", program_options::value< uint32_t >()   , "The contract entry point for override mode")
         (CONTRACT_ID_OPTION       ",i", program_options::value< std::string >(), "The contract ID for override mode")
      ;

      program_options::variables_map args;
      program_options::store( program_options::parse_command_line( argc, argv, options ), args );

      koinos::initialize_logging("koinos_contract_uploader", {}, "info" );

      if ( args.count( HELP_OPTION ) )
      {
         std::cout << options << std::endl;
         return EXIT_SUCCESS;
      }

      auto client = std::make_shared< mq::client >();

      auto ec = client->connect( args[ AMQP_OPTION ].as< std::string >(), mq::retry_policy::none );

      if ( ec != mq::error_code::success )
         KOINOS_THROW( koinos::exception, "Unable to connect to AMQP server " );

      std::filesystem::path private_key_file( args[ PRIVATE_KEY_FILE_OPTION ].as< std::string >() );

      KOINOS_ASSERT(
         std::filesystem::exists( private_key_file ),
         koinos::exception,
         "Unable to find private key file at: ${loc}", ("loc", private_key_file.string())
      );

      std::ifstream ifs( private_key_file );
      std::string private_key_wif( ( std::istreambuf_iterator< char >( ifs ) ), ( std::istreambuf_iterator< char >() ) );

      crypto::private_key signing_key = crypto::private_key::from_wif( private_key_wif );
      std::string public_address      = signing_key.get_public_key().to_address();

      protocol::transaction transaction;
      transaction.active_data.make_mutable();

      if ( args.count( UPLOAD_OPTION ) )
      {
         std::filesystem::path contract_file = args[ CONTRACT_OPTION ].as< std::string >();

         KOINOS_ASSERT(
            std::filesystem::exists( contract_file ),
            koinos::exception,
            "Unable to find contract file at: ${loc}", ("loc", contract_file.string())
         );

         std::ifstream contract( contract_file, std::ios::binary );

         std::vector< char > bytecode( (std::istreambuf_iterator< char >( contract )), std::istreambuf_iterator< char >() );

         protocol::upload_contract_operation op;
         auto contract_id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, signing_key.get_public_key().to_address() );
         std::memcpy( op.contract_id.data(), contract_id.digest.data(), op.contract_id.size() );
         op.bytecode.insert( op.bytecode.end(), bytecode.begin(), bytecode.end() );

         transaction.active_data->operations.push_back( op );
         transaction.active_data->resource_limit = 10'000'000;
         transaction.active_data->nonce = get_next_nonce( client, public_address );

         pack::json j;
         pack::to_json( j, op.contract_id );
         LOG(info) << "Attempting to upload contract with ID: " << j.dump();
      }
      else if ( args.count( OVERRIDE_OPTION ) )
      {
         KOINOS_ASSERT(
            args.count( CALL_ID_OPTION ),
            koinos::exception,
            "The call ID option is required"
         );

         KOINOS_ASSERT(
            args.count( ENTRY_POINT_OPTION ),
            koinos::exception,
            "The entry point is required"
         );

         KOINOS_ASSERT(
            args.count( CONTRACT_ID_OPTION ),
            koinos::exception,
            "The contract ID is required"
         );

         auto j = koinos::pack::json::parse( '"' + args[ CONTRACT_ID_OPTION ].as< std::string >() + '"' );
         chain::contract_call_bundle bundle;
         koinos::pack::from_json( j, bundle.contract_id );

         bundle.entry_point = args[ ENTRY_POINT_OPTION ].as< uint32_t >();

         protocol::set_system_call_operation op;
         op.call_id = args[ CALL_ID_OPTION ].as< uint32_t >();
         op.target  = bundle;

         transaction.active_data->operations.push_back( op );
         transaction.active_data->resource_limit = 10'000'000;
         transaction.active_data->nonce = get_next_nonce( client, public_address );

         LOG(info) << "Attempting to apply the system call override";
      }
      else
      {
         KOINOS_THROW( koinos::exception, "Use --upload or --override when invoking the tool, see --help for more information" );
      }

      transaction.id = crypto::hash( CRYPTO_SHA2_256_ID, transaction.active_data );
      auto signature = signing_key.sign_compact( transaction.id );
      pack::to_variable_blob( transaction.signature_data, signature );

      submit_transaction( client, transaction );

      LOG(info) << "Transaction successfully submitted";

      return EXIT_SUCCESS;
   }
   catch ( const std::exception& e )
   {
      LOG(fatal) << e.what() << std::endl;
   }
   catch ( const boost::exception& e )
   {
      LOG(fatal) << boost::diagnostic_information( e ) << std::endl;
   }
   catch ( ... )
   {
      LOG(fatal) << "unknown exception" << std::endl;
   }

   return EXIT_FAILURE;
}

uint64_t get_next_nonce( std::shared_ptr< mq::client > client, const std::string& account )
{
   uint64_t nonce = 0;

   rpc::chain::chain_rpc_request req = rpc::chain::get_account_nonce_request {
      .account = protocol::account_type( account.begin(), account.end() )
   };

   pack::json j;
   pack::to_json( j, req );
   auto future = client->rpc( service::chain, j.dump(), 750 /* ms */, mq::retry_policy::none );

   rpc::chain::chain_rpc_response resp;
   pack::from_json( pack::json::parse( future.get() ), resp );

   std::visit( koinos::overloaded {
      [&]( const rpc::chain::get_account_nonce_response& r )
      {
         nonce = r.nonce;
      },
      [&] ( const rpc::chain::chain_error_response& r )
      {
         KOINOS_THROW( koinos::exception, "Received error response from chain: ${error}", ("error", r.error_text) );
      },
      [&] ( const auto& r )
      {
         KOINOS_THROW( koinos::exception, "Unexpected response from chain" );
      }
   }, resp );

   return nonce;
}

void submit_transaction( std::shared_ptr< mq::client > client, const protocol::transaction& t )
{
   rpc::chain::chain_rpc_request req = rpc::chain::submit_transaction_request {
      .transaction = t,
      .verify_passive_data = true,
      .verify_transaction_signatures = true
   };

   pack::json j;
   pack::to_json( j, req );
   auto future = client->rpc( service::chain, j.dump(), 750 /* ms */, mq::retry_policy::none );

   rpc::chain::chain_rpc_response resp;
   pack::from_json( pack::json::parse( future.get() ), resp );

   std::visit( koinos::overloaded {
      [&]( const rpc::chain::submit_transaction_response& r )
      {
      },
      [&] ( const rpc::chain::chain_error_response& r )
      {
         KOINOS_THROW( koinos::exception, "Received error response from chain: ${error}", ("error", r.error_text) );
      },
      [&] ( const auto& r )
      {
         KOINOS_THROW( koinos::exception, "Unexpected response from chain" );
      }
   }, resp );
}
