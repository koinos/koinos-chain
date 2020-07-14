#include <boost/test/unit_test.hpp>

#include <koinos/chain/controller.hpp>
#include <koinos/crypto/multihash.hpp>
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

BOOST_AUTO_TEST_SUITE_END()
