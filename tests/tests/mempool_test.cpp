#include <boost/test/unit_test.hpp>

#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/host.hpp>
#include <koinos/chain/mempool.hpp>
#include <koinos/chain/thunks.hpp>

#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>

#include <koinos/pack/classes.hpp>

#include <mira/database_configuration.hpp>

#include <memory>

using namespace koinos;

struct mempool_fixture
{
   mempool_fixture()
   {
      _ctx = std::make_shared< chain::apply_context >();
      _ctx->privilege_level = chain::privilege::kernel_mode;

      _temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
      boost::filesystem::create_directory( _temp );
      std::any cfg = mira::utilities::default_database_configuration();

      _db.open( _temp, cfg );
      _ctx->set_state_node( _db.create_writable_node( _db.get_head()->id(), koinos::crypto::hash( CRYPTO_SHA2_256_ID, 1 ) ) );

      chain::register_host_functions();

      _host_api = std::make_unique< chain::host_api >( *_ctx );

      std::string seed1 = "alpha beta gamma delta";
      _key1 = crypto::private_key::generate_from_seed( crypto::hash( CRYPTO_SHA2_256_ID, seed1 ) );

      std::string seed2 = "echo foxtrot golf hotel";
      _key2 = crypto::private_key::generate_from_seed( crypto::hash( CRYPTO_SHA2_256_ID, seed2 ) );
   }

   ~mempool_fixture()
   {
      _db.close();
      boost::filesystem::remove_all( _temp );
   }

   multihash sign( crypto::private_key& key, protocol::transaction& t )
   {
      auto digest = crypto::hash( CRYPTO_SHA2_256_ID, t.active_data.get_const_native() );
      auto signature = key.sign_compact( digest );
      t.signature_data = variable_blob( signature.begin(), signature.end() );
      return digest;
   }

   boost::filesystem::path                 _temp;
   statedb::state_db                       _db;

   std::unique_ptr< chain::host_api >      _host_api;
   std::shared_ptr< chain::apply_context > _ctx;

   crypto::private_key                     _key1;
   crypto::private_key                     _key2;
};

BOOST_FIXTURE_TEST_SUITE( mempool_tests, mempool_fixture )

BOOST_AUTO_TEST_CASE( mempool_basic_test )
{
   chain::mempool mempool;

   protocol::transaction t1;
   t1.active_data->resource_limit = 10;
   auto t1_id = sign( _key1, t1 );

   BOOST_TEST_MESSAGE( "adding pending transaction" );
   auto payer = chain::thunk::get_transaction_payer( *_ctx, t1 );
   auto max_payer_resources = chain::thunk::get_max_account_resources( *_ctx, payer );
   auto trx_resource_limit = chain::thunk::get_transaction_resource_limit( *_ctx, t1 );
   mempool.add_pending_transaction( t1_id, t1, block_height_type{ 1 }, payer, max_payer_resources, trx_resource_limit );

   BOOST_TEST_MESSAGE( "adding duplicate pending transaction" );
   BOOST_REQUIRE_THROW( mempool.add_pending_transaction( t1_id, t1, block_height_type{ 2 }, payer, max_payer_resources, trx_resource_limit ), chain::pending_transaction_insertion_failure );

   BOOST_TEST_MESSAGE( "checking pending transaction list" );
   {
      auto pending_txs = mempool.get_pending_transactions();
      BOOST_TEST_MESSAGE( "checking pending transactions size" );
      BOOST_REQUIRE_EQUAL( pending_txs.size(), 1 );
      BOOST_TEST_MESSAGE( "checking pending transaction id" );
      BOOST_REQUIRE_EQUAL( crypto::hash( CRYPTO_SHA2_256_ID, pending_txs[0].active_data.get_const_native() ), t1_id );
   }

   BOOST_TEST_MESSAGE( "pending transaction existence check" );
   BOOST_REQUIRE_EQUAL( mempool.has_pending_transaction( t1_id ), true );

   protocol::transaction t2;
   t2.active_data->resource_limit = 1000000000000;
   auto t2_id = sign( _key1, t2 );

   BOOST_TEST_MESSAGE( "adding pending transaction that exceeds accout resources" );
   payer = chain::thunk::get_transaction_payer( *_ctx, t2 );
   max_payer_resources = chain::thunk::get_max_account_resources( *_ctx, payer );
   trx_resource_limit = chain::thunk::get_transaction_resource_limit( *_ctx, t2 );
   BOOST_REQUIRE_THROW( mempool.add_pending_transaction( t2_id, t2, block_height_type{ 3 }, payer, max_payer_resources, trx_resource_limit ), chain::transaction_exceeds_resources );

   BOOST_TEST_MESSAGE( "removing pending transaction" );
   mempool.remove_pending_transaction( t1_id );

   BOOST_TEST_MESSAGE( "checking pending transaction list" );
   {
      auto pending_txs = mempool.get_pending_transactions();
      BOOST_TEST_MESSAGE( "checking pending transactions size" );
      BOOST_REQUIRE_EQUAL( pending_txs.size(), 0 );
   }

   BOOST_TEST_MESSAGE( "pending transaction existence check" );
   BOOST_REQUIRE_EQUAL( mempool.has_pending_transaction( t1_id ), false );
}

BOOST_AUTO_TEST_SUITE_END()
