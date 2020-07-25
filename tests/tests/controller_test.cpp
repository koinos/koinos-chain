#include <boost/test/unit_test.hpp>

#include <koinos/chain/controller.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/crypto/elliptic.hpp>
#include <koinos/exception.hpp>
#include <koinos/pack/rt/binary.hpp>
#include <koinos/plugins/block_producer/util/block_util.hpp>

#include <mira/database_configuration.hpp>

#include <chrono>

using namespace koinos;
using koinos::plugins::block_producer::util::set_block_merkle_roots;
using koinos::plugins::block_producer::util::sign_block;

struct controller_fixture
{
   controller_fixture()
   {
      temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
      boost::filesystem::create_directory( temp );
   }

   ~controller_fixture()
   {
      controller.stop_threads();
      boost::filesystem::remove_all( temp );
   }

   chain::controller       controller;
   boost::filesystem::path temp;
};

BOOST_FIXTURE_TEST_SUITE( controller_tests, controller_fixture )

BOOST_AUTO_TEST_CASE( setup_tests )
{ try {

   BOOST_TEST_MESSAGE( "Test when threads have not been started" );

   auto future = controller.submit( types::rpc::query_submission( types::rpc::get_head_info_params() ) );
   auto status = future.wait_for( std::chrono::milliseconds( 50 ) );
   BOOST_CHECK( status == std::future_status::timeout );

   BOOST_TEST_MESSAGE( "Start threads" );
   controller.start_threads();

   auto submit_res = *(future.get());
   auto& query_err = std::get< types::rpc::submission_error_result >( submit_res );
   std::string error_str( query_err.error_text.data(), query_err.error_text.size() );
   BOOST_CHECK_EQUAL( error_str, "Database is not open" );

   BOOST_TEST_MESSAGE( "Check success with open database" );

   std::any cfg = mira::utilities::default_database_configuration();
   controller.open( temp, cfg );
   controller.set_time( std::chrono::steady_clock::now() );

   future = controller.submit( types::rpc::query_submission( types::rpc::get_head_info_params() ) );
   submit_res = *(future.get());
   auto& query_res = std::get< types::rpc::query_submission_result >( submit_res );
   query_res.unbox();
   query_res.unlock();
   auto& head_info_res = std::get< types::rpc::get_head_info_result >( query_res.get_native() );

   BOOST_CHECK_EQUAL( head_info_res.height, 0 );
   BOOST_CHECK_EQUAL( head_info_res.id, crypto::zero_hash( CRYPTO_SHA2_256_ID ) );

   BOOST_TEST_MESSAGE( "Stop threads" );

   controller.stop_threads();
   future = controller.submit( types::rpc::query_submission( types::rpc::get_head_info_params() ) );
   BOOST_CHECK_THROW( future.get(), std::future_error );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( submission_tests )
{ try {
   std::any cfg = mira::utilities::default_database_configuration();
   controller.open( temp, cfg );
   controller.start_threads();

   std::string seed = "test seed";
   auto block_signing_private_key = crypto::private_key::regenerate( crypto::hash_str( CRYPTO_SHA2_256_ID, seed.c_str(), seed.size() ) );

   BOOST_TEST_MESSAGE( "Test reserved submission" );

   BOOST_CHECK_THROW( controller.submit( types::rpc::reserved_submission() ), chain::unknown_submission_type );


   BOOST_TEST_MESSAGE( "Test reserved query" );

   auto future = controller.submit( types::rpc::query_submission( types::rpc::reserved_query_params() ) );
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

   future = controller.submit( trx );
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

   block_submission.topology.id = crypto::hash_blob( CRYPTO_SHA2_256_ID, block_submission.block.active_data );

   future = controller.submit( block_submission );
   submit_res = *(future.get());
   auto& submit_err = std::get< types::rpc::submission_error_result >( submit_res );
   error_str = std::string( submit_err.error_text.data(), submit_err.error_text.size() );
   BOOST_CHECK_EQUAL( error_str, "First block must have height of 1" );


   BOOST_TEST_MESSAGE( "Error when signature does not match" );

   block_submission.block.active_data->height = 1;
   block_submission.topology.height = block_submission.block.active_data->height;
   block_submission.topology.id = crypto::hash_blob( CRYPTO_SHA2_256_ID, block_submission.block.active_data );

   future = controller.submit( block_submission );
   submit_res = *(future.get());
   submit_err = std::get< types::rpc::submission_error_result >( submit_res );
   error_str = std::string( submit_err.error_text.data(), submit_err.error_text.size() );
   BOOST_CHECK_EQUAL( error_str, "Block signature does not match" );


   BOOST_TEST_MESSAGE( "Error when previous block does not match" );

   block_submission.topology.previous = crypto::empty_hash( CRYPTO_SHA2_256_ID );
   block_submission.block.active_data->header_hashes.digests[(uint32_t)types::protocol::header_hash_index::previous_block_hash_index] = block_submission.topology.previous.digest;

   set_block_merkle_roots( block_submission.block, CRYPTO_SHA2_256_ID );
   sign_block( block_submission.block, block_signing_private_key );

   future = controller.submit( block_submission );
   submit_res = *(future.get());
   submit_err = std::get< types::rpc::submission_error_result >( submit_res );
   error_str = std::string( submit_err.error_text.data(), submit_err.error_text.size() );
   BOOST_CHECK_EQUAL( error_str, "Unknown previous block" );


   BOOST_TEST_MESSAGE( "Test succesful block" );

   block_submission.topology.previous = crypto::zero_hash( CRYPTO_SHA2_256_ID );
   block_submission.block.active_data->header_hashes.digests[(uint32_t)types::protocol::header_hash_index::previous_block_hash_index] = block_submission.topology.previous.digest;

   set_block_merkle_roots( block_submission.block, CRYPTO_SHA2_256_ID );
   sign_block( block_submission.block, block_signing_private_key );

   future = controller.submit( block_submission );
   submit_res = *(future.get());
   auto block_res = std::get< types::rpc::block_submission_result >( submit_res );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_SUITE_END()
