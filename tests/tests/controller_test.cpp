#include <boost/test/unit_test.hpp>


#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <boost/program_options.hpp>

#include <koinos/crypto/multihash.hpp>
#include <koinos/crypto/elliptic.hpp>
#include <koinos/exception.hpp>
#include <koinos/pack/rt/binary.hpp>
#include <koinos/plugins/block_producer/util/block_util.hpp>
#include <koinos/plugins/chain/chain_plugin.hpp>
#include <koinos/plugins/chain/reqhandler.hpp>

#include <mira/database_configuration.hpp>

#include <chrono>
#include <sstream>

using namespace koinos;
using koinos::plugins::block_producer::util::set_block_merkle_roots;
using koinos::plugins::block_producer::util::sign_block;

/**
 * We want to test the chain_plugin behavior when we've specified --state-dir=path/to/temp/dir
 *
 * It would be very simple if we could simply say:
 *
 *    boost::program_options::variables_map vm;
 *    vm["state-dir"] = _state_dir.string();
 *
 * Unfortunately, variable_map can only be initialized by a parser, so the only way to achieve this is to write the option to an in-memory config file.
 *
 * See https://stackoverflow.com/questions/56056265/insert-into-boostprogram-optionsvariables-map-by-index-operator
 *
 */
boost::program_options::variables_map create_program_options(
   appbase::abstract_plugin& plugin,
   const std::vector< std::pair< std::string, std::string > >& args
   )
{
   namespace bpo = boost::program_options;

   bpo::options_description cli_options("");
   bpo::options_description cfg_options("");

   plugin.set_program_options( cli_options, cfg_options );

   bpo::variables_map vm;

   std::stringstream config;

   for( const auto& kv : args )
   {
      config << kv.first << "=" << kv.second << std::endl;
   }

   bpo::store(bpo::parse_config_file(config, cfg_options, true), vm);
   bpo::notify(vm);

   return vm;
}

struct reqhandler_fixture
{
   reqhandler_fixture()
   {
      _state_dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
      boost::filesystem::create_directory( _state_dir );

      _options = create_program_options(
         _chain_plugin,
         {
            {"state-dir", _state_dir.string()},
            {"database-config", "database.cfg"},
            {"mq-disable", "true"}
         }
      );
   }

   virtual ~reqhandler_fixture()
   {
      boost::filesystem::remove_all( _state_dir );
   }

   boost::program_options::variables_map    _options;
   plugins::chain::chain_plugin             _chain_plugin;
   boost::filesystem::path                  _state_dir;
};

BOOST_FIXTURE_TEST_SUITE( reqhandler_tests, reqhandler_fixture )

BOOST_AUTO_TEST_CASE( setup_tests )
{ try {

   BOOST_TEST_MESSAGE( "Test when chain_plugin has not been started" );

   auto future = _chain_plugin.submit( types::rpc::query_submission( types::rpc::get_head_info_params() ) );
   auto status = future.wait_for( std::chrono::milliseconds( 50 ) );
   BOOST_CHECK( status == std::future_status::timeout );

   BOOST_TEST_MESSAGE( "Start chain_plugin" );
   _chain_plugin.plugin_initialize( _options );
   _chain_plugin.plugin_startup();

   BOOST_TEST_MESSAGE( "Check success with chain_plugin started" );

   future = _chain_plugin.submit( types::rpc::query_submission( types::rpc::get_head_info_params() ) );
   auto submit_res = *(future.get());
   auto& query_res = std::get< types::rpc::query_submission_result >( submit_res );
   query_res.make_mutable();
   auto& head_info_res = std::get< types::rpc::get_head_info_result >( query_res.get_native() );

   BOOST_CHECK_EQUAL( head_info_res.height, 0 );
   BOOST_CHECK_EQUAL( head_info_res.id, crypto::zero_hash( CRYPTO_SHA2_256_ID ) );

   BOOST_TEST_MESSAGE( "Shut down chain_plugin" );

   _chain_plugin.plugin_shutdown();
   future = _chain_plugin.submit( types::rpc::query_submission( types::rpc::get_head_info_params() ) );
   BOOST_CHECK_THROW( future.get(), std::future_error );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( submission_tests )
{ try {
   using koinos::plugins::chain::unknown_submission_type;
   _chain_plugin.plugin_initialize( _options );
   _chain_plugin.plugin_startup();
   std::string seed = "test seed";
   auto block_signing_private_key = crypto::private_key::regenerate( crypto::hash_str( CRYPTO_SHA2_256_ID, seed.c_str(), seed.size() ) );

   BOOST_TEST_MESSAGE( "Test reserved submission" );

   BOOST_CHECK_THROW( _chain_plugin.submit( types::rpc::reserved_submission() ), unknown_submission_type );


   BOOST_TEST_MESSAGE( "Test reserved query" );

   auto future = _chain_plugin.submit( types::rpc::query_submission( types::rpc::reserved_query_params() ) );
   auto submit_res = *(future.get());
   auto& query_err = std::get< types::rpc::query_error >( std::get< types::rpc::query_submission_result >( submit_res ).get_const_native() );
   std::string error_str( query_err.error_text.data(), query_err.error_text.size() );
   BOOST_CHECK_EQUAL( error_str, "Unimplemented query type" );

   BOOST_TEST_MESSAGE( "Test submit transaction" );

   types::protocol::operation o = types::protocol::nop_operation();
   types::protocol::transaction transaction;
   transaction.operations.push_back( o );

   types::rpc::transaction_submission trx;
   pack::to_variable_blob( trx.active_bytes, transaction );

   future = _chain_plugin.submit( trx );
   submit_res = *(future.get());
   auto& trx_res = std::get< types::rpc::transaction_submission_result >( submit_res );


   BOOST_TEST_MESSAGE( "Test submit block" );
   BOOST_TEST_MESSAGE( "Error when first block does not have height of 1" );

   types::rpc::block_submission block_submission;
   block_submission.verify_passive_data = true;
   block_submission.verify_block_signature = true;
   block_submission.verify_transaction_signatures = true;

   auto duration = std::chrono::system_clock::now().time_since_epoch();
   block_submission.block.active_data->timestamp = std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count();
   block_submission.block.active_data->height = 2;
   block_submission.block.active_data->header_hashes.digests.resize(3);

   block_submission.topology.previous = crypto::zero_hash( CRYPTO_SHA2_256_ID );
   block_submission.topology.height = block_submission.block.active_data->height;
   block_submission.block.active_data->header_hashes.digests[(uint32_t)types::protocol::header_hash_index::previous_block_hash_index] = block_submission.topology.previous.digest;

   set_block_merkle_roots( block_submission.block, CRYPTO_SHA2_256_ID );
   sign_block( block_submission.block, block_signing_private_key );

   block_submission.topology.id = crypto::hash( CRYPTO_SHA2_256_ID, block_submission.block.active_data );

   future = _chain_plugin.submit( block_submission );
   submit_res = *(future.get());
   auto& submit_err = std::get< types::rpc::submission_error_result >( submit_res );
   error_str = std::string( submit_err.error_text.data(), submit_err.error_text.size() );
   BOOST_CHECK_EQUAL( error_str, "First block must have height of 1" );


   BOOST_TEST_MESSAGE( "Error when signature does not match" );

   block_submission.block.active_data.make_mutable();
   block_submission.block.active_data->height = 1;
   block_submission.topology.height = block_submission.block.active_data->height;
   block_submission.topology.id = crypto::hash( CRYPTO_SHA2_256_ID, block_submission.block.active_data );

   future = _chain_plugin.submit( block_submission );
   submit_res = *(future.get());
   submit_err = std::get< types::rpc::submission_error_result >( submit_res );
   error_str = std::string( submit_err.error_text.data(), submit_err.error_text.size() );
   BOOST_CHECK_EQUAL( error_str, "Block signature does not match" );


   BOOST_TEST_MESSAGE( "Error when previous block does not match" );

   block_submission.topology.previous = crypto::empty_hash( CRYPTO_SHA2_256_ID );
   block_submission.block.active_data.make_mutable();
   block_submission.block.active_data->header_hashes.digests[(uint32_t)types::protocol::header_hash_index::previous_block_hash_index] = block_submission.topology.previous.digest;

   set_block_merkle_roots( block_submission.block, CRYPTO_SHA2_256_ID );
   sign_block( block_submission.block, block_signing_private_key );

   future = _chain_plugin.submit( block_submission );
   submit_res = *(future.get());
   submit_err = std::get< types::rpc::submission_error_result >( submit_res );
   error_str = std::string( submit_err.error_text.data(), submit_err.error_text.size() );
   BOOST_CHECK_EQUAL( error_str, "Unknown previous block" );


   BOOST_TEST_MESSAGE( "Test succesful block" );

   block_submission.topology.previous = crypto::zero_hash( CRYPTO_SHA2_256_ID );
   block_submission.block.active_data.make_mutable();
   block_submission.block.active_data->header_hashes.digests[(uint32_t)types::protocol::header_hash_index::previous_block_hash_index] = block_submission.topology.previous.digest;

   set_block_merkle_roots( block_submission.block, CRYPTO_SHA2_256_ID );
   sign_block( block_submission.block, block_signing_private_key );

   future = _chain_plugin.submit( block_submission );
   submit_res = *(future.get());
   auto block_res = std::get< types::rpc::block_submission_result >( submit_res );

   BOOST_TEST_MESSAGE( "Test chain ID retrieval" );
   future = _chain_plugin.submit( types::rpc::query_submission( types::rpc::get_chain_id_params() ) );
   submit_res = *(future.get());
   auto query_submission_result = std::get< types::rpc::query_submission_result >( submit_res );
   auto chain_id_result = std::get< types::rpc::get_chain_id_result >( query_submission_result.get_const_native() );
   std::string chain_id = "koinos";
   BOOST_CHECK_EQUAL( chain_id_result.chain_id, crypto::hash_str( CRYPTO_SHA2_256_ID, chain_id.data(), chain_id.size() ) );

   _chain_plugin.plugin_shutdown();
} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_SUITE_END()
