#include <boost/test/unit_test.hpp>

#include <koinos/chain/controller.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/crypto/elliptic.hpp>
#include <koinos/exception.hpp>
#include <koinos/pack/rt/binary.hpp>

#include <mira/database_configuration.hpp>

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

   koinos::chain::controller  controller;
   boost::filesystem::path    temp;
};

BOOST_FIXTURE_TEST_SUITE( controller_tests, controller_fixture )

BOOST_AUTO_TEST_CASE( setup_tests )
{ try {

   BOOST_TEST_MESSAGE( "Test when threads have not been started" );
   koinos::types::rpc::submission_item submit_item;
   koinos::types::rpc::query_submission query;
   koinos::types::rpc::query_param_item query_item = koinos::types::rpc::get_head_info_params();
   koinos::pack::to_variable_blob( query.query, query_item );
   submit_item = query;

   auto future = controller.submit( submit_item );
   auto status = future.wait_for( std::chrono::milliseconds( 50 ) );
   BOOST_CHECK( status == std::future_status::timeout );

   BOOST_TEST_MESSAGE( "Start threads" );
   controller.start_threads();

   auto submit_res = *(future.get());
   auto& query_err = std::get< koinos::types::rpc::submission_error_result >( submit_res );
   std::string error_str( query_err.error_text.data(), query_err.error_text.size() );
   BOOST_CHECK_EQUAL( error_str, "Database is not open" );

   BOOST_TEST_MESSAGE( "Check success with open database" );

   std::any cfg = mira::utilities::default_database_configuration();
   controller.open( temp, cfg );

   future = controller.submit( submit_item );
   submit_res = *(future.get());
   auto& query_res = std::get< koinos::types::rpc::query_submission_result >( submit_res );
   auto query_item_res = koinos::pack::from_variable_blob< koinos::types::rpc::query_item_result >( query_res.result );
   auto& head_info_res = std::get< koinos::types::rpc::get_head_info_result >( query_item_res );

   BOOST_CHECK_EQUAL( head_info_res.height, 0 );
   BOOST_CHECK_EQUAL( head_info_res.id, koinos::crypto::zero_hash( CRYPTO_SHA2_256_ID ) );

   BOOST_TEST_MESSAGE( "Stop threads" );

   controller.stop_threads();
   future = controller.submit( submit_item );
   BOOST_CHECK_THROW( future.get(), std::future_error );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( submission_tests )
{ try {
   std::any cfg = mira::utilities::default_database_configuration();
   controller.open( temp, cfg );
   controller.start_threads();

   std::string seed = "test seed";
   auto block_signing_private_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash_str( CRYPTO_SHA2_256_ID, seed.c_str(), seed.size() ) );

   BOOST_TEST_MESSAGE( "Test reserved submission" );

   koinos::types::rpc::submission_item submit_item;
   submit_item = koinos::types::rpc::reserved_submission();

   BOOST_CHECK_THROW( controller.submit( submit_item ), koinos::chain::unknown_submission_type );


   BOOST_TEST_MESSAGE( "Test submit transaction" );

   koinos::types::protocol::operation o = koinos::types::protocol::nop_operation();
   koinos::types::protocol::transaction_type transaction;
   transaction.operations.push_back( koinos::pack::to_variable_blob( o ) );

   koinos::types::rpc::transaction_submission trx;
   koinos::pack::to_variable_blob( trx.active_bytes, transaction );

   auto future = controller.submit( trx );
   auto submit_res = *(future.get());
   auto& trx_res = std::get< koinos::types::rpc::transaction_submission_result >( submit_res );


   BOOST_TEST_MESSAGE( "Test submit block" );
   BOOST_TEST_MESSAGE( "Error when first block does not have height of 1" );

   koinos::types::protocol::active_block_data active_data;
   auto duration = std::chrono::system_clock::now().time_since_epoch();
   active_data.timestamp = std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count();
   active_data.height = 2;

   koinos::types::rpc::block_topology topology;
   topology.previous = koinos::crypto::zero_hash( CRYPTO_SHA2_256_ID );
   topology.height = active_data.height;

   koinos::types::protocol::passive_block_data passive_data;
   passive_data.block_signature = block_signing_private_key.sign_compact( koinos::crypto::hash( CRYPTO_SHA2_256_ID, active_data ) );

   koinos::types::protocol::block_header block;
   koinos::crypto::hash( block.passive_merkle_root, CRYPTO_SHA2_256_ID, passive_data );
   koinos::pack::to_variable_blob( block.active_bytes, active_data );
   topology.id = koinos::crypto::hash( CRYPTO_SHA2_256_ID, block );

   koinos::types::rpc::block_submission block_submission;
   block_submission.topology = topology;
   koinos::pack::to_variable_blob( block_submission.header_bytes, block );
   block_submission.passives_bytes.emplace_back( koinos::pack::to_variable_blob( passive_data ) );

   future = controller.submit( block_submission );
   submit_res = *(future.get());
   auto submit_err = std::get< koinos::types::rpc::submission_error_result >( submit_res );
   std::string error_str( submit_err.error_text.data(), submit_err.error_text.size() );
   BOOST_CHECK_EQUAL( error_str, "First block must have height of 1" );

   BOOST_TEST_MESSAGE( "Error when signature does not match" );

   active_data.height = 1;
   topology.height = active_data.height;
   koinos::pack::to_variable_blob( block.active_bytes, active_data );
   koinos::pack::to_variable_blob( block_submission.header_bytes, block );
   topology.id = koinos::crypto::hash( CRYPTO_SHA2_256_ID, block );
   block_submission.topology = topology;
   block_submission.passives_bytes.clear();
   block_submission.passives_bytes.emplace_back( koinos::pack::to_variable_blob( passive_data ) );

   future = controller.submit( block_submission );
   submit_res = *(future.get());
   submit_err = std::get< koinos::types::rpc::submission_error_result >( submit_res );
   error_str = std::string( submit_err.error_text.data(), submit_err.error_text.size() );
   BOOST_CHECK_EQUAL( error_str, "invalid block signature" );

   BOOST_TEST_MESSAGE( "Error when previous block does not match" );

   topology.previous = koinos::crypto::hash( CRYPTO_SHA2_256_ID, 1 );
   passive_data.block_signature = block_signing_private_key.sign_compact( koinos::crypto::hash( CRYPTO_SHA2_256_ID, active_data ) );
   block_submission.topology = topology;
   block_submission.passives_bytes.clear();
   block_submission.passives_bytes.emplace_back( koinos::pack::to_variable_blob( passive_data ) );

   future = controller.submit( block_submission );
   submit_res = *(future.get());
   submit_err = std::get< koinos::types::rpc::submission_error_result >( submit_res );
   error_str = std::string( submit_err.error_text.data(), submit_err.error_text.size() );
   BOOST_CHECK_EQUAL( error_str, "Unknown previous block" );

   BOOST_TEST_MESSAGE( "Test succesful block" );

   topology.previous = koinos::crypto::zero_hash( CRYPTO_SHA2_256_ID );
   block_submission.topology = topology;

   future = controller.submit( block_submission );
   submit_res = *(future.get());
   auto block_res = std::get< koinos::types::rpc::block_submission_result >( submit_res );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_SUITE_END()
