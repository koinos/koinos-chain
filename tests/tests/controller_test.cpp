#include <boost/test/unit_test.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <koinos/chain/constants.hpp>
#include <koinos/chain/controller.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/crypto/elliptic.hpp>

#include <mira/database_configuration.hpp>

// #include <koinos/tests/koin.hpp>
// #include <koinos/tests/wasm/contract_return.hpp>
// #include <koinos/tests/wasm/db_write.hpp>
// #include <koinos/tests/wasm/hello.hpp>
// #include <koinos/tests/wasm/koin.hpp>

#include <chrono>
#include <filesystem>
#include <sstream>

using namespace koinos;
using namespace std::string_literals;

struct controller_fixture
{
   controller_fixture()
   {
      initialize_logging( "koinos_test", {}, "info" );

      auto seed = "test seed"s;
      _block_signing_private_key = crypto::private_key::regenerate( crypto::hash( koinos::crypto::multicodec::sha2_256, seed.c_str(), seed.size() ) );

      _state_dir = std::filesystem::temp_directory_path() / boost::filesystem::unique_path().string();
      LOG(info) << "Test temp dir: " << _state_dir.string();
      std::filesystem::create_directory( _state_dir );

      auto database_config = mira::utilities::default_database_configuration();

      chain::genesis_data genesis_data;
      auto chain_id = crypto::hash( koinos::crypto::multicodec::sha2_256, _block_signing_private_key.get_public_key().to_address() );
      genesis_data[ { converter::as< statedb::object_space >( chain::database::space::kernel ), converter::as< statedb::object_key >( chain::database::key::chain_id ) } ] = converter::as< std::vector< std::byte > >( chain_id );

      _controller.open( _state_dir, database_config, genesis_data, false );
   }

   virtual ~controller_fixture()
   {
      std::filesystem::remove_all( _state_dir );
   }

   void set_block_merkle_roots( protocol::block& block, protocol::active_block_data& active_data, crypto::multicodec code, std::size_t size = 0 )
   {
      std::vector< crypto::multihash > transactions;
      std::vector< crypto::multihash > passives;
      transactions.reserve( block.transactions().size() );
      passives.reserve( 2 * ( block.transactions().size() + 1 ) );

      passives.emplace_back( crypto::hash( code, block.passive(), size ) );
      passives.emplace_back( crypto::multihash::empty( code ) );

      for ( const auto& trx : block.transactions() )
      {
         passives.emplace_back( crypto::hash( code, trx.passive(), size ) );
         passives.emplace_back( crypto::hash( code, trx.signature_data(), size ) );
         transactions.emplace_back( crypto::hash( code, trx, size ) );
      }

      auto transaction_merkle_tree = crypto::merkle_tree( code, transactions );
      auto passives_merkle_tree = crypto::merkle_tree( code, passives );

      active_data.set_transaction_merkle_root( converter::as< std::string >( transaction_merkle_tree.root()->hash() ) );
      active_data.set_passive_data_merkle_root( converter::as< std::string >( passives_merkle_tree.root()->hash() ) );
   }

   void sign_block( protocol::block& block, crypto::private_key& block_signing_key )
   {
      auto id_mh = crypto::hash( crypto::multicodec::sha2_256, block.header(), block.active() );
      block.set_signature_data( converter::as< std::string >( block_signing_key.sign_compact( id_mh ) ) );
   }

   void sign_transaction( protocol::transaction& transaction, crypto::private_key& transaction_signing_key )
   {
      // Signature is on the hash of the active data
      auto id_mh = crypto::hash( crypto::multicodec::sha2_256, transaction.active() );
      transaction.set_id( converter::as< std::string >( id_mh ) );
      transaction.set_signature_data( converter::as< std::string >( transaction_signing_key.sign_compact( id_mh ) ) );
   }
/*
   std::vector< uint8_t > get_hello_wasm()
   {
      return std::vector< uint8_t >( hello_wasm, hello_wasm + hello_wasm_len );
   }

   std::vector< uint8_t > get_contract_return_wasm()
   {
      return std::vector< uint8_t >( contract_return_wasm, contract_return_wasm + contract_return_wasm_len );
   }

   std::vector< uint8_t > get_db_write_wasm()
   {
      return std::vector< uint8_t >( db_write_wasm, db_write_wasm + db_write_wasm_len );
   }

   std::vector< uint8_t > get_koin_wasm()
   {
      return std::vector< uint8_t >( koin_wasm, koin_wasm + koin_wasm_len );
   }
*/
   chain::controller      _controller;
   std::filesystem::path  _state_dir;
   crypto::private_key    _block_signing_private_key;
};

BOOST_FIXTURE_TEST_SUITE( controller_tests, controller_fixture )

BOOST_AUTO_TEST_CASE( submission_tests )
{ try {
   using namespace koinos;

   BOOST_TEST_MESSAGE( "Test submit transaction" );

   auto key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "foobar"s ) );
   rpc::chain::submit_transaction_request trx_req;

   protocol::active_transaction_data trx_active_data;

   BOOST_TEST_MESSAGE( "Test submit block" );
   BOOST_TEST_MESSAGE( "Error when first block does not have height of 1" );

   rpc::chain::submit_block_request block_req;
   block_req.set_verify_passive_data( true );
   block_req.set_verify_block_signature( true );
   block_req.set_verify_transaction_signature( true );

   auto duration = std::chrono::system_clock::now().time_since_epoch();
   block_req.mutable_block()->mutable_header()->set_timestamp( std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count() );
   block_req.mutable_block()->mutable_header()->set_height( 2 );
   block_req.mutable_block()->mutable_header()->set_previous( converter::as< std::string >( crypto::multihash::zero( crypto::multicodec::sha2_256 ) ) );
   block_req.mutable_block()->mutable_passive();

   protocol::active_block_data block_active_data;

   set_block_merkle_roots( *block_req.mutable_block(), block_active_data, crypto::multicodec::sha2_256 );
   block_req.mutable_block()->set_active( converter::as< std::string >( block_active_data ) );
   sign_block( *block_req.mutable_block(), _block_signing_private_key );

   block_req.mutable_block()->set_id( converter::as< std::string >( koinos::crypto::hash( crypto::multicodec::sha2_256, block_req.block().header(), block_req.block().active() ) ) );

   BOOST_CHECK_THROW( _controller.submit_block( block_req ), chain::unexpected_height );

   BOOST_TEST_MESSAGE( "Error when signature does not match" );

   auto foo_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash( crypto::multicodec::sha2_256, "foo"s ) );

   block_active_data.set_signer( converter::as< std::string >( foo_key.get_public_key().to_address() ) );
   block_req.mutable_block()->mutable_header()->set_height( 1 );
   block_req.mutable_block()->set_id( converter::as< std::string >( koinos::crypto::hash( crypto::multicodec::sha2_256, block_req.block().header(), block_req.block().active() ) ) );

   sign_block( *block_req.mutable_block(), foo_key );

   BOOST_CHECK_THROW( _controller.submit_block( block_req ), chain::invalid_block_signature );

   BOOST_TEST_MESSAGE( "Error when previous block does not match" );

   block_req.mutable_block()->mutable_header()->set_previous( converter::as< std::string >( crypto::multihash::empty( crypto::multicodec::sha2_256 ) ) );

   set_block_merkle_roots( *block_req.mutable_block(), block_active_data, crypto::multicodec::sha2_256 );
   block_req.mutable_block()->set_active( converter::as< std::string >( block_active_data ) );
   block_req.mutable_block()->set_id( converter::as< std::string >( koinos::crypto::hash( crypto::multicodec::sha2_256, block_req.block().header(), block_req.block().active() ) ) );
   sign_block( *block_req.mutable_block(), _block_signing_private_key );

   BOOST_CHECK_THROW( _controller.submit_block( block_req ), chain::unknown_previous_block );

   BOOST_TEST_MESSAGE( "Error when block timestamp is too far in the future" );

   duration = ( std::chrono::system_clock::now() + std::chrono::minutes( 1 ) ).time_since_epoch();
   block_req.mutable_block()->mutable_header()->set_timestamp( std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count() );
   block_req.mutable_block()->mutable_header()->set_previous( converter::as< std::string >( crypto::multihash::zero( crypto::multicodec::sha2_256 ) ) );
   block_req.mutable_block()->set_id( converter::as< std::string >( koinos::crypto::hash( crypto::multicodec::sha2_256, block_req.block().header(), block_req.block().active() ) ) );

   BOOST_CHECK_THROW( _controller.submit_block( block_req ), chain::timestamp_out_of_bounds );

   duration = std::chrono::system_clock::now().time_since_epoch();
   block_req.mutable_block()->mutable_header()->set_timestamp( std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count() );

   BOOST_TEST_MESSAGE( "Test successful block" );

   block_req.mutable_block()->mutable_header()->set_previous( converter::as< std::string >( crypto::multihash::zero( crypto::multicodec::sha2_256 ) ) );

   set_block_merkle_roots( *block_req.mutable_block(), block_active_data, crypto::multicodec::sha2_256 );
   block_req.mutable_block()->set_active( converter::as< std::string >( block_active_data ) );
   block_req.mutable_block()->set_id( converter::as< std::string >( koinos::crypto::hash( crypto::multicodec::sha2_256, block_req.block().header(), block_req.block().active() ) ) );
   sign_block( *block_req.mutable_block(), _block_signing_private_key );

   _controller.submit_block( block_req );

   BOOST_TEST_MESSAGE( "Error when block is too old" );

   block_req.mutable_block()->mutable_header()->set_previous( block_req.block().id() );
   block_req.mutable_block()->mutable_header()->set_height( 2 );
   block_req.mutable_block()->mutable_header()->set_timestamp( block_req.block().header().timestamp() - 1 );

   set_block_merkle_roots( *block_req.mutable_block(), block_active_data, crypto::multicodec::sha2_256 );
   block_req.mutable_block()->set_active( converter::as< std::string >( block_active_data ) );
   block_req.mutable_block()->set_id( converter::as< std::string >( koinos::crypto::hash( crypto::multicodec::sha2_256, block_req.block().header(), block_req.block().active() ) ) );
   sign_block( *block_req.mutable_block(), _block_signing_private_key );

   BOOST_CHECK_THROW( _controller.submit_block( block_req ), chain::timestamp_out_of_bounds );

   BOOST_TEST_MESSAGE( "Test chain ID retrieval" );

   BOOST_CHECK_EQUAL(
      converter::to< crypto::multihash >( _controller.get_chain_id().chain_id() ),
      koinos::crypto::hash( crypto::multicodec::sha2_256, _block_signing_private_key.get_public_key().to_address() )
   );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( block_irreversibility )
{ try {

   BOOST_TEST_MESSAGE( "Check block irreversibility" );

   rpc::chain::submit_block_request block_req;
   block_req.set_verify_passive_data( true );
   block_req.set_verify_block_signature( true );
   block_req.set_verify_transaction_signature( true );

   auto head_info_res = _controller.get_head_info();

   for ( uint64_t i = 1; i <= chain::default_irreversible_threshold; i++ )
   {
      auto duration = std::chrono::system_clock::now().time_since_epoch();
      block_req.mutable_block()->mutable_header()->set_timestamp( std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count() );
      block_req.mutable_block()->mutable_header()->set_height( head_info_res.head_topology().height() + 1 );
      block_req.mutable_block()->mutable_header()->set_previous( head_info_res.head_topology().id() );

      protocol::active_block_data active_data;
      set_block_merkle_roots( *block_req.mutable_block(), active_data, koinos::crypto::multicodec::sha2_256 );
      block_req.mutable_block()->set_active( converter::as< std::string >( active_data ) );
      block_req.mutable_block()->set_id( converter::as< std::string >( crypto::hash( koinos::crypto::multicodec::sha2_256, block_req.block().header(), block_req.block().active() ) ) );
      sign_block( *block_req.mutable_block(), _block_signing_private_key );

      _controller.submit_block( block_req );

      head_info_res = _controller.get_head_info();

      BOOST_REQUIRE( head_info_res.last_irreversible_block() == 0 );
   }

   for ( uint64_t i = chain::default_irreversible_threshold + 1; i <= chain::default_irreversible_threshold + 3; i++ )
   {
      auto duration = std::chrono::system_clock::now().time_since_epoch();
      block_req.mutable_block()->mutable_header()->set_timestamp( std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count() );
      block_req.mutable_block()->mutable_header()->set_height( head_info_res.head_topology().height() + 1 );
      block_req.mutable_block()->mutable_header()->set_previous( head_info_res.head_topology().id() );

      protocol::active_block_data active_data;
      set_block_merkle_roots( *block_req.mutable_block(), active_data, koinos::crypto::multicodec::sha2_256 );
      block_req.mutable_block()->set_active( converter::as< std::string >( active_data ) );
      block_req.mutable_block()->set_id( converter::as< std::string >( crypto::hash( koinos::crypto::multicodec::sha2_256, block_req.block().header(), block_req.block().active() ) ) );
      sign_block( *block_req.mutable_block(), _block_signing_private_key );

      _controller.submit_block( block_req );

      head_info_res = _controller.get_head_info();

      BOOST_REQUIRE( head_info_res.last_irreversible_block() == i - chain::default_irreversible_threshold );
   }

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( fork_heads )
{ try {
   BOOST_TEST_MESSAGE( "Setting up forks and checking heads" );

   rpc::chain::submit_block_request block_req;
   block_req.set_verify_passive_data( true );
   block_req.set_verify_block_signature( true );
   block_req.set_verify_transaction_signature( true );

   uint64_t test_timestamp = 1609459200;

   auto root_head_info = _controller.get_head_info();
   rpc::chain::get_head_info_response head_info;
   head_info.CopyFrom( root_head_info );

   for ( uint64_t i = 1; i <= chain::default_irreversible_threshold; i++ )
   {
      block_req.mutable_block()->mutable_header()->set_timestamp( test_timestamp + i );
      block_req.mutable_block()->mutable_header()->set_height( head_info.head_topology().height() + 1 );
      block_req.mutable_block()->mutable_header()->set_previous( head_info.head_topology().id() );

      protocol::active_block_data active_data;
      set_block_merkle_roots( *block_req.mutable_block(), active_data, koinos::crypto::multicodec::sha2_256 );
      block_req.mutable_block()->set_active( converter::as< std::string >( active_data ) );
      block_req.mutable_block()->set_id( converter::as< std::string >( crypto::hash( koinos::crypto::multicodec::sha2_256, block_req.block().header(), block_req.block().active() ) ) );
      sign_block( *block_req.mutable_block(), _block_signing_private_key );

      _controller.submit_block( block_req );

      head_info.mutable_head_topology()->set_height( head_info.head_topology().height() + 1 );
      head_info.mutable_head_topology()->set_previous( head_info.head_topology().id() );
      head_info.mutable_head_topology()->set_id( block_req.block().id() );
   }

   rpc::chain::get_head_info_response fork_head_info;
   fork_head_info.CopyFrom( head_info );
   head_info.CopyFrom( root_head_info );

   for ( uint64_t i = 1; i <= chain::default_irreversible_threshold; i++ )
   {
      block_req.mutable_block()->mutable_header()->set_timestamp( test_timestamp + i + chain::default_irreversible_threshold );
      block_req.mutable_block()->mutable_header()->set_height( head_info.head_topology().height() + 1 );
      block_req.mutable_block()->mutable_header()->set_previous( head_info.head_topology().id() );

      protocol::active_block_data active_data;
      set_block_merkle_roots( *block_req.mutable_block(), active_data, koinos::crypto::multicodec::sha2_256 );
      block_req.mutable_block()->set_active( converter::as< std::string >( active_data ) );
      block_req.mutable_block()->set_id( converter::as< std::string >( crypto::hash( koinos::crypto::multicodec::sha2_256, block_req.block().header(), block_req.block().active() ) ) );
      sign_block( *block_req.mutable_block(), _block_signing_private_key );

      _controller.submit_block( block_req );

      head_info.mutable_head_topology()->set_height( head_info.head_topology().height() + 1 );
      head_info.mutable_head_topology()->set_previous( head_info.head_topology().id() );
      head_info.mutable_head_topology()->set_id( block_req.block().id() );

      auto fork_heads = _controller.get_fork_heads();

      BOOST_REQUIRE_EQUAL( fork_heads.fork_heads_size(), 2 );
      auto topo0 = fork_heads.fork_heads( 0 );
      auto topo1 = fork_heads.fork_heads( 1 );

      BOOST_CHECK( (    topo0.height()   == fork_head_info.head_topology().height()
                     && topo0.previous() == fork_head_info.head_topology().previous()
                     && topo0.id()       == fork_head_info.head_topology().id()
                     && topo1.height()   == head_info.head_topology().height()
                     && topo1.previous() == head_info.head_topology().previous()
                     && topo1.id()       == head_info.head_topology().id() )
                   || ( topo1.height()   == fork_head_info.head_topology().height()
                     && topo1.previous() == fork_head_info.head_topology().previous()
                     && topo1.id()       == fork_head_info.head_topology().id()
                     && topo0.height()   == head_info.head_topology().height()
                     && topo0.previous() == head_info.head_topology().previous()
                     && topo0.id()       == head_info.head_topology().id() ) );
   }

   block_req.mutable_block()->mutable_header()->set_timestamp( test_timestamp + ( 2 * chain::default_irreversible_threshold ) + 1 );
   block_req.mutable_block()->mutable_header()->set_height( head_info.head_topology().height() + 1 );
   block_req.mutable_block()->mutable_header()->set_previous( head_info.head_topology().id() );

   protocol::active_block_data active_data;
   set_block_merkle_roots( *block_req.mutable_block(), active_data, koinos::crypto::multicodec::sha2_256 );
   block_req.mutable_block()->set_active( converter::as< std::string >( active_data ) );
   block_req.mutable_block()->set_id( converter::as< std::string >( crypto::hash( koinos::crypto::multicodec::sha2_256, block_req.block().header(), block_req.block().active() ) ) );
   sign_block( *block_req.mutable_block(), _block_signing_private_key );

   _controller.submit_block( block_req );

   head_info.mutable_head_topology()->set_height( head_info.head_topology().height() + 1 );
   head_info.mutable_head_topology()->set_previous( head_info.head_topology().id() );
   head_info.mutable_head_topology()->set_id( block_req.block().id() );

   auto fork_heads = _controller.get_fork_heads();

   BOOST_REQUIRE_EQUAL( fork_heads.fork_heads_size(), 1 );
   BOOST_CHECK_EQUAL( fork_heads.fork_heads( 0 ).height(), head_info.head_topology().height() );
   BOOST_CHECK_EQUAL( fork_heads.fork_heads( 0 ).previous(), head_info.head_topology().previous() );
   BOOST_CHECK_EQUAL( fork_heads.fork_heads( 0 ).id(), head_info.head_topology().id() );


} KOINOS_CATCH_LOG_AND_RETHROW(info) }
#if 0
BOOST_AUTO_TEST_CAS E( read_contract_tests )
{ try {
   BOOST_TEST_MESSAGE( "Upload contracts" );

   auto key1 = koinos::crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "foobar1"s ) );
   auto key2 = koinos::crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "foobar2"s ) );
   auto key3 = koinos::crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "foobar3"s ) );

   koinos::protocol::transaction trx1;
   trx1.active_data.make_mutable();

   koinos::protocol::upload_contract_operation op;
   auto contract_1_id = koinos::crypto::hash( koinos::crypto::multicodec::ripemd_160, key1.get_public_key().to_address() );
   std::memcpy( op.contract_id.data(), contract_1_id.digest.data(), op.contract_id.size() );
   auto bytecode = get_hello_wasm();
   op.bytecode.insert( op.bytecode.end(), bytecode.begin(), bytecode.end() );
   trx1.active_data->operations.push_back( op );
   trx1.active_data->resource_limit = KOINOS_MAX_METER_TICKS;
   sign_transaction( trx1, key1 );

   // Upload the return test contract
   koinos::protocol::transaction trx2;
   trx2.active_data.make_mutable();

   bytecode = get_contract_return_wasm();
   auto contract_2_id = koinos::crypto::hash( koinos::crypto::multicodec::ripemd_160, key2.get_public_key().to_address() );
   std::memcpy( op.contract_id.data(), contract_2_id.digest.data(), op.contract_id.size() );
   op.bytecode.clear();
   op.bytecode.insert( op.bytecode.end(), bytecode.begin(), bytecode.end() );
   trx2.active_data->operations.push_back( op );
   trx2.active_data->resource_limit = KOINOS_MAX_METER_TICKS;
   sign_transaction( trx2, key2 );

   // Upload the db write contract
   koinos::protocol::transaction trx3;
   trx3.active_data.make_mutable();

   bytecode = get_db_write_wasm();
   auto contract_3_id = koinos::crypto::hash( koinos::crypto::multicodec::ripemd_160, key3.get_public_key().to_address() );
   std::memcpy( op.contract_id.data(), contract_3_id.digest.data(), op.contract_id.size() );
   op.bytecode.clear();
   op.bytecode.insert( op.bytecode.end(), bytecode.begin(), bytecode.end() );
   trx3.active_data->operations.push_back( op );
   trx3.active_data->resource_limit = KOINOS_MAX_METER_TICKS;
   sign_transaction( trx3, key3 );

   koinos::rpc::chain::submit_block_request block_req;
   block_req.verify_passive_data = false;
   block_req.verify_block_signature = false;
   block_req.verify_transaction_signatures = false;

   auto duration = std::chrono::system_clock::now().time_since_epoch();
   block_req.block.header.timestamp = std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count();
   block_req.block.header.height = 1;
   block_req.block.header.previous = koinos::crypto::multihash::zero( koinos::crypto::multicodec::sha2_256 );
   block_req.block.transactions.push_back( trx1 );
   block_req.block.transactions.push_back( trx2 );
   block_req.block.transactions.push_back( trx3 );

   set_block_merkle_roots( block_req.block, koinos::crypto::multicodec::sha2_256 );
   sign_block( block_req.block, _block_signing_private_key );

   block_req.block.id = koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, block_req.block.header, block_req.block.active_data );

   _controller.submit_block( block_req );


   BOOST_TEST_MESSAGE( "Test read contract logs" );

   koinos::rpc::chain::read_contract_request request;
   std::memcpy( request.contract_id.data(), contract_1_id.digest.data(), request.contract_id.size() );
   request.entry_point = 0;
   request.args = koinos::variable_blob();

   auto response = _controller.read_contract( request );

   BOOST_REQUIRE( response.result.size() == 0 );
   BOOST_REQUIRE( response.logs == "Greetings from koinos vm" );


   BOOST_TEST_MESSAGE( "Test read contract return" );

   std::string arg_str = "echo";
   koinos::variable_blob args = koinos::pack::to_variable_blob( arg_str );

   std::memcpy( request.contract_id.data(), contract_2_id.digest.data(), request.contract_id.size() );
   request.args = args;

   response = _controller.read_contract( request );

   auto return_str = koinos::pack::from_variable_blob< std::string >( response.result );
   BOOST_REQUIRE( args.size() == response.result.size() );
   BOOST_REQUIRE( std::equal( args.begin(), args.end(), response.result.begin() ) );


   BOOST_TEST_MESSAGE( "Test read contract db write" );

   std::memcpy( request.contract_id.data(), contract_3_id.digest.data(), request.contract_id.size() );
   BOOST_REQUIRE_THROW( _controller.read_contract( request ), koinos::chain::read_only_context );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CAS E( transaction_reversion_test )
{ try {
   BOOST_TEST_MESSAGE( "Upload KOIN contract and attempt to mint to Alice" );

   auto contract_private_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "contract"s ) );
   auto alice_private_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "alice"s ) );
   auto alice_address = alice_private_key.get_public_key().to_address();
   koinos::protocol::transaction trx1;
   trx1.active_data.make_mutable();

   // Upload the KOIN contract
   koinos::protocol::upload_contract_operation op;
   auto id = koinos::crypto::hash( koinos::crypto::multicodec::ripemd_160, contract_private_key.get_public_key().to_address() );
   std::memcpy( op.contract_id.data(), id.digest.data(), op.contract_id.size() );
   auto bytecode = get_koin_wasm();
   op.bytecode.insert( op.bytecode.end(), bytecode.begin(), bytecode.end() );
   trx1.active_data->operations.push_back( op );
   trx1.active_data->resource_limit = KOINOS_MAX_METER_TICKS;
   sign_transaction( trx1, contract_private_key );

   koinos::protocol::transaction trx2;
   trx2.active_data.make_mutable();

   auto m_args = mint_args{ .to = alice_address, .value = 100 };
   koinos::protocol::call_contract_operation call;
   std::memcpy( call.contract_id.data(), id.digest.data(), call.contract_id.size() );
   call.entry_point = 0xc2f82bdc;
   call.args = koinos::pack::to_variable_blob( m_args );
   trx2.active_data->resource_limit = KOINOS_MAX_METER_TICKS;
   trx2.active_data->operations.push_back( call );
   sign_transaction( trx2, alice_private_key );

   koinos::rpc::chain::submit_block_request block_req;
   block_req.verify_passive_data = false;
   block_req.verify_block_signature = false;
   block_req.verify_transaction_signatures = false;

   auto duration = std::chrono::system_clock::now().time_since_epoch();
   block_req.block.header.timestamp = std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count();
   block_req.block.header.height = 1;
   block_req.block.header.previous = koinos::crypto::multihash::zero( koinos::crypto::multicodec::sha2_256 );
   block_req.block.transactions.push_back( trx1 );
   block_req.block.transactions.push_back( trx2 );

   set_block_merkle_roots( block_req.block, koinos::crypto::multicodec::sha2_256 );
   sign_block( block_req.block, _block_signing_private_key );

   block_req.block.id = koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, block_req.block.header, block_req.block.active_data );

   _controller.submit_block( block_req );

   BOOST_TEST_MESSAGE( "Verify mint did nothing" );

   koinos::rpc::chain::read_contract_request request;
   std::memcpy( request.contract_id.data(), id.digest.data(), request.contract_id.size() );
   request.entry_point = 0x15619248;
   request.args = koinos::pack::to_variable_blob( alice_address );

   LOG(info) << request;

   auto response = _controller.read_contract( request );
   auto alice_balance = koinos::pack::from_variable_blob< uint64_t >( response.result );
   BOOST_REQUIRE_EQUAL( alice_balance, 0 );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }
#endif
BOOST_AUTO_TEST_SUITE_END()
