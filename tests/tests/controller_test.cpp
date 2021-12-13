#include <boost/test/unit_test.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <koinos/chain/constants.hpp>
#include <koinos/chain/controller.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/crypto/elliptic.hpp>

#include <koinos/tests/wasm/contract_return.hpp>
#include <koinos/tests/wasm/db_write.hpp>
#include <koinos/tests/wasm/hello.hpp>
#include <koinos/tests/wasm/koin.hpp>

#include <koinos/contracts/token/token.pb.h>

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

      chain::genesis_data genesis_data;
      auto chain_id = crypto::hash( koinos::crypto::multicodec::sha2_256, _block_signing_private_key.get_public_key().to_address_bytes() );
      genesis_data[ { chain::state::space::metadata(), chain::state::key::chain_id } ] = util::converter::as< state_db::object_value >( chain_id );

      _controller.open( _state_dir, genesis_data, false );
   }

   virtual ~controller_fixture()
   {
      boost::log::core::get()->remove_all_sinks();
      std::filesystem::remove_all( _state_dir );
   }

   void set_block_merkle_roots( protocol::block& block, crypto::multicodec code, crypto::digest_size size = crypto::digest_size( 0 ) )
   {
      std::vector< crypto::multihash > transactions;
      transactions.reserve( block.transactions().size() );

      for ( const auto& trx : block.transactions() )
      {
         transactions.emplace_back( crypto::hash( code, trx.header(), size ) );
      }

      auto transaction_merkle_tree = crypto::merkle_tree( code, transactions );
      block.mutable_header()->set_transaction_merkle_root( util::converter::as< std::string >( transaction_merkle_tree.root()->hash() ) );
   }

   void sign_block( protocol::block& block, crypto::private_key& block_signing_key )
   {
      auto id_mh = crypto::hash( crypto::multicodec::sha2_256, block.header() );
      block.set_signature( util::converter::as< std::string >( block_signing_key.sign_compact( id_mh ) ) );
   }

   void set_transaction_merkle_roots( protocol::transaction& transaction, crypto::multicodec code, crypto::digest_size size = crypto::digest_size( 0 ) )
   {
      std::vector< crypto::multihash > operations;
      operations.reserve( transaction.operations().size() );

      for ( const auto& op : transaction.operations() )
      {
         operations.emplace_back( crypto::hash( code, op, size ) );
      }

      auto operation_merkle_tree = crypto::merkle_tree( code, operations );
      transaction.mutable_header()->set_operation_merkle_root( util::converter::as< std::string >( operation_merkle_tree.root()->hash() ) );
   }

   void sign_transaction( protocol::transaction& transaction, crypto::private_key& transaction_signing_key )
   {
      auto id_mh = crypto::hash( crypto::multicodec::sha2_256, transaction.header() );
      transaction.set_signature( util::converter::as< std::string >( transaction_signing_key.sign_compact( id_mh ) ) );
   }

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

   BOOST_TEST_MESSAGE( "Test submit block" );
   BOOST_TEST_MESSAGE( "Error when first block does not have height of 1" );

   rpc::chain::submit_block_request block_req;

   auto duration = std::chrono::system_clock::now().time_since_epoch();
   block_req.mutable_block()->mutable_header()->set_timestamp( std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count() );
   block_req.mutable_block()->mutable_header()->set_height( 2 );
   block_req.mutable_block()->mutable_header()->set_previous_state_merkle_root( _controller.get_head_info().head_state_merkle_root() );
   block_req.mutable_block()->mutable_header()->set_previous( util::converter::as< std::string >( crypto::multihash::zero( crypto::multicodec::sha2_256 ) ) );

   set_block_merkle_roots( *block_req.mutable_block(), crypto::multicodec::sha2_256 );
   sign_block( *block_req.mutable_block(), _block_signing_private_key );

   block_req.mutable_block()->set_id( util::converter::as< std::string >( koinos::crypto::hash( crypto::multicodec::sha2_256, block_req.block().header() ) ) );

   BOOST_CHECK_THROW( _controller.submit_block( block_req ), chain::unexpected_height );

   BOOST_TEST_MESSAGE( "Error when signature does not match" );

   auto foo_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash( crypto::multicodec::sha2_256, "foo"s ) );

   block_req.mutable_block()->mutable_header()->set_signer( util::converter::as< std::string >( foo_key.get_public_key().to_address_bytes() ) );
   block_req.mutable_block()->mutable_header()->set_height( 1 );
   block_req.mutable_block()->mutable_header()->set_previous_state_merkle_root( _controller.get_head_info().head_state_merkle_root() );
   block_req.mutable_block()->set_id( util::converter::as< std::string >( koinos::crypto::hash( crypto::multicodec::sha2_256, block_req.block().header() ) ) );

   sign_block( *block_req.mutable_block(), foo_key );

   BOOST_CHECK_THROW( _controller.submit_block( block_req ), chain::invalid_block_signature );

   BOOST_TEST_MESSAGE( "Error when previous block does not match" );

   block_req.mutable_block()->mutable_header()->set_previous( util::converter::as< std::string >( crypto::multihash::empty( crypto::multicodec::sha2_256 ) ) );

   set_block_merkle_roots( *block_req.mutable_block(), crypto::multicodec::sha2_256 );
   block_req.mutable_block()->set_id( util::converter::as< std::string >( koinos::crypto::hash( crypto::multicodec::sha2_256, block_req.block().header() ) ) );
   sign_block( *block_req.mutable_block(), _block_signing_private_key );

   BOOST_CHECK_THROW( _controller.submit_block( block_req ), chain::unknown_previous_block );

   BOOST_TEST_MESSAGE( "Error when block timestamp is too far in the future" );

   duration = ( std::chrono::system_clock::now() + std::chrono::minutes( 1 ) ).time_since_epoch();
   block_req.mutable_block()->mutable_header()->set_timestamp( std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count() );
   block_req.mutable_block()->mutable_header()->set_previous( util::converter::as< std::string >( crypto::multihash::zero( crypto::multicodec::sha2_256 ) ) );
   block_req.mutable_block()->set_id( util::converter::as< std::string >( koinos::crypto::hash( crypto::multicodec::sha2_256, block_req.block().header() ) ) );

   BOOST_CHECK_THROW( _controller.submit_block( block_req ), chain::timestamp_out_of_bounds );

   duration = std::chrono::system_clock::now().time_since_epoch();
   block_req.mutable_block()->mutable_header()->set_timestamp( std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count() );

   BOOST_TEST_MESSAGE( "Test successful block" );

   block_req.mutable_block()->mutable_header()->set_previous( util::converter::as< std::string >( crypto::multihash::zero( crypto::multicodec::sha2_256 ) ) );

   set_block_merkle_roots( *block_req.mutable_block(), crypto::multicodec::sha2_256 );
   block_req.mutable_block()->set_id( util::converter::as< std::string >( koinos::crypto::hash( crypto::multicodec::sha2_256, block_req.block().header() ) ) );
   sign_block( *block_req.mutable_block(), _block_signing_private_key );

   _controller.submit_block( block_req );

   BOOST_TEST_MESSAGE( "Error when block is too old" );

   block_req.mutable_block()->mutable_header()->set_previous( block_req.block().id() );
   block_req.mutable_block()->mutable_header()->set_height( 2 );
   block_req.mutable_block()->mutable_header()->set_timestamp( block_req.block().header().timestamp() - 1 );

   set_block_merkle_roots( *block_req.mutable_block(), crypto::multicodec::sha2_256 );
   block_req.mutable_block()->set_id( util::converter::as< std::string >( koinos::crypto::hash( crypto::multicodec::sha2_256, block_req.block().header() ) ) );
   sign_block( *block_req.mutable_block(), _block_signing_private_key );

   BOOST_CHECK_THROW( _controller.submit_block( block_req ), chain::timestamp_out_of_bounds );

   BOOST_TEST_MESSAGE( "Test chain ID retrieval" );

   BOOST_CHECK_EQUAL(
      util::converter::to< crypto::multihash >( _controller.get_chain_id().chain_id() ),
      koinos::crypto::hash( crypto::multicodec::sha2_256, _block_signing_private_key.get_public_key().to_address_bytes() )
   );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( block_irreversibility )
{ try {

   BOOST_TEST_MESSAGE( "Check block irreversibility" );

   rpc::chain::submit_block_request block_req;

   auto head_info_res = _controller.get_head_info();

   for ( uint64_t i = 1; i <= chain::default_irreversible_threshold; i++ )
   {
      auto duration = std::chrono::system_clock::now().time_since_epoch();
      block_req.mutable_block()->mutable_header()->set_timestamp( std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count() );
      block_req.mutable_block()->mutable_header()->set_height( head_info_res.head_topology().height() + 1 );
      block_req.mutable_block()->mutable_header()->set_previous( head_info_res.head_topology().id() );
      block_req.mutable_block()->mutable_header()->set_previous_state_merkle_root( _controller.get_head_info().head_state_merkle_root() );

      set_block_merkle_roots( *block_req.mutable_block(), koinos::crypto::multicodec::sha2_256 );
      block_req.mutable_block()->set_id( util::converter::as< std::string >( crypto::hash( koinos::crypto::multicodec::sha2_256, block_req.block().header() ) ) );
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
      block_req.mutable_block()->mutable_header()->set_previous_state_merkle_root( _controller.get_head_info().head_state_merkle_root() );

      set_block_merkle_roots( *block_req.mutable_block(), koinos::crypto::multicodec::sha2_256 );
      block_req.mutable_block()->set_id( util::converter::as< std::string >( crypto::hash( koinos::crypto::multicodec::sha2_256, block_req.block().header() ) ) );
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

   uint64_t test_timestamp = 1609459200;

   auto root_head_info = _controller.get_head_info();
   rpc::chain::get_head_info_response head_info;
   head_info.CopyFrom( root_head_info );

   const std::string first_state_merkle_root = _controller.get_head_info().head_state_merkle_root();
   std::string last_state_merkle_root = first_state_merkle_root;
   for ( uint64_t i = 1; i <= chain::default_irreversible_threshold; i++ )
   {
      block_req.mutable_block()->mutable_header()->set_timestamp( test_timestamp + i );
      block_req.mutable_block()->mutable_header()->set_height( head_info.head_topology().height() + 1 );
      block_req.mutable_block()->mutable_header()->set_previous( head_info.head_topology().id() );
      block_req.mutable_block()->mutable_header()->set_previous_state_merkle_root( _controller.get_head_info().head_state_merkle_root() );

      set_block_merkle_roots( *block_req.mutable_block(), koinos::crypto::multicodec::sha2_256 );
      block_req.mutable_block()->set_id( util::converter::as< std::string >( crypto::hash( koinos::crypto::multicodec::sha2_256, block_req.block().header() ) ) );
      sign_block( *block_req.mutable_block(), _block_signing_private_key );

      auto receipt = _controller.submit_block( block_req ).receipt();
      last_state_merkle_root = receipt.state_merkle_root();

      head_info.mutable_head_topology()->set_height( head_info.head_topology().height() + 1 );
      head_info.mutable_head_topology()->set_previous( head_info.head_topology().id() );
      head_info.mutable_head_topology()->set_id( block_req.block().id() );
   }

   rpc::chain::get_head_info_response fork_head_info;
   fork_head_info.CopyFrom( head_info );
   head_info.CopyFrom( root_head_info );

   last_state_merkle_root = first_state_merkle_root;
   for ( uint64_t i = 1; i <= chain::default_irreversible_threshold; i++ )
   {
      block_req.mutable_block()->mutable_header()->set_timestamp( test_timestamp + i + chain::default_irreversible_threshold );
      block_req.mutable_block()->mutable_header()->set_height( head_info.head_topology().height() + 1 );
      block_req.mutable_block()->mutable_header()->set_previous( head_info.head_topology().id() );
      block_req.mutable_block()->mutable_header()->set_previous_state_merkle_root( last_state_merkle_root );

      set_block_merkle_roots( *block_req.mutable_block(), koinos::crypto::multicodec::sha2_256 );
      block_req.mutable_block()->set_id( util::converter::as< std::string >( crypto::hash( koinos::crypto::multicodec::sha2_256, block_req.block().header() ) ) );
      sign_block( *block_req.mutable_block(), _block_signing_private_key );

      auto receipt = _controller.submit_block( block_req ).receipt();
      last_state_merkle_root = receipt.state_merkle_root();

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
   block_req.mutable_block()->mutable_header()->set_previous_state_merkle_root( last_state_merkle_root );

   set_block_merkle_roots( *block_req.mutable_block(), koinos::crypto::multicodec::sha2_256 );
   block_req.mutable_block()->set_id( util::converter::as< std::string >( crypto::hash( koinos::crypto::multicodec::sha2_256, block_req.block().header() ) ) );
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

BOOST_AUTO_TEST_CASE( read_contract_tests )
{ try {
   BOOST_TEST_MESSAGE( "Upload contracts" );

   auto key1 = koinos::crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "foobar1"s ) );
   auto key2 = koinos::crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "foobar2"s ) );
   auto key3 = koinos::crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "foobar3"s ) );

   koinos::protocol::transaction trx1;

   auto op1 = trx1.add_operations()->mutable_upload_contract();
   op1->set_contract_id( util::converter::as< std::string >( key1.get_public_key().to_address_bytes() ) );
   op1->set_bytecode( util::converter::as< std::string >( get_hello_wasm() ) );
   trx1.mutable_header()->set_rc_limit( 10'000'000 );
   set_transaction_merkle_roots( trx1, koinos::crypto::multicodec::sha2_256 );
   trx1.set_id( util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, trx1.header() ) ) );
   sign_transaction( trx1, key1 );

   // Upload the return test contract
   koinos::protocol::transaction trx2;

   auto op2 = trx2.add_operations()->mutable_upload_contract();
   op2->set_contract_id( util::converter::as< std::string >( key2.get_public_key().to_address_bytes() ) );
   op2->set_bytecode( util::converter::as< std::string >( get_contract_return_wasm() ) );
   trx2.mutable_header()->set_rc_limit( 10'000'000 );
   set_transaction_merkle_roots( trx2, koinos::crypto::multicodec::sha2_256 );
   trx2.set_id( util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, trx2.header() ) ) );
   sign_transaction( trx2, key2 );

   // Upload the db write contract
   koinos::protocol::transaction trx3;

   auto op3 = trx3.add_operations()->mutable_upload_contract();
   op3->set_contract_id( util::converter::as< std::string >( key3.get_public_key().to_address_bytes() ) );
   op3->set_bytecode( util::converter::as< std::string >( get_db_write_wasm() ) );
   trx3.mutable_header()->set_rc_limit( 10'000'000 );
   set_transaction_merkle_roots( trx3, koinos::crypto::multicodec::sha2_256 );
   trx3.set_id( util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, trx3.header() ) ) );
   sign_transaction( trx3, key3 );

   koinos::rpc::chain::submit_block_request block_req;

   auto duration = std::chrono::system_clock::now().time_since_epoch();
   block_req.mutable_block()->mutable_header()->set_timestamp( std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count() );
   block_req.mutable_block()->mutable_header()->set_height( 1 );
   block_req.mutable_block()->mutable_header()->set_previous( util::converter::as< std::string >( koinos::crypto::multihash::zero( koinos::crypto::multicodec::sha2_256 ) ) );
   block_req.mutable_block()->mutable_header()->set_previous_state_merkle_root( _controller.get_head_info().head_state_merkle_root() );
   *block_req.mutable_block()->add_transactions() = trx1;
   *block_req.mutable_block()->add_transactions() = trx2;
   *block_req.mutable_block()->add_transactions() = trx3;

   set_block_merkle_roots( *block_req.mutable_block(), koinos::crypto::multicodec::sha2_256 );
   block_req.mutable_block()->set_id( util::converter::as< std::string >( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, block_req.block().header() ) ) );
   sign_block( *block_req.mutable_block(), _block_signing_private_key );

   _controller.submit_block( block_req );

   BOOST_TEST_MESSAGE( "Test read contract logs" );

   koinos::rpc::chain::read_contract_request request;
   request.set_contract_id( util::converter::as< std::string >( key1.get_public_key().to_address_bytes() ) );
   request.set_entry_point( 0 );

   auto response = _controller.read_contract( request );

   BOOST_REQUIRE( response.result().size() == 0 );
   BOOST_REQUIRE( response.logs() == "Greetings from koinos vm" );

   BOOST_TEST_MESSAGE( "Test read contract return" );

   request.set_contract_id( util::converter::as< std::string >( key2.get_public_key().to_address_bytes() ) );
   request.set_args( "echo" );

   response = _controller.read_contract( request );

   auto return_str = response.result();
   BOOST_REQUIRE( std::string( "echo" ).size() == response.result().size() );
   BOOST_REQUIRE( std::string( "echo" ) == response.result() );


   BOOST_TEST_MESSAGE( "Test read contract db write" );

   request.set_contract_id( util::converter::as< std::string >( key3.get_public_key().to_address_bytes() ) );
   BOOST_REQUIRE_THROW( _controller.read_contract( request ), koinos::chain::read_only_context );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( transaction_reversion_test )
{ try {
   BOOST_TEST_MESSAGE( "Upload KOIN contract and attempt to mint to Alice" );

   auto contract_private_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "contract"s ) );
   auto alice_private_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "alice"s ) );
   auto alice_address = alice_private_key.get_public_key().to_address_bytes();
   koinos::protocol::transaction trx1;

   // Upload the KOIN contract
   auto op1 = trx1.add_operations()->mutable_upload_contract();
   op1->set_contract_id( util::converter::as< std::string >( contract_private_key.get_public_key().to_address_bytes() ) );
   op1->set_bytecode( util::converter::as< std::string >( get_koin_wasm() ) );
   trx1.mutable_header()->set_rc_limit( 10'000'000 );
   set_transaction_merkle_roots( trx1, koinos::crypto::multicodec::sha2_256 );
   trx1.set_id( util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, trx1.header() ) ) );
   sign_transaction( trx1, contract_private_key );

   koinos::protocol::transaction trx2;

   koinos::contracts::token::mint_arguments mint_arg;
   mint_arg.set_to( alice_address );
   mint_arg.set_value( 100 );

   auto op2 = trx2.add_operations()->mutable_call_contract();
   op2->set_contract_id( op1->contract_id() );
   op2->set_entry_point( 0xc2f82bdc );
   op2->set_args( mint_arg.SerializeAsString() );
   trx2.mutable_header()->set_rc_limit( 10'000'000 );
   set_transaction_merkle_roots( trx2, koinos::crypto::multicodec::sha2_256 );
   trx2.set_id( util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, trx2.header() ) ) );
   sign_transaction( trx2, alice_private_key );

   koinos::rpc::chain::submit_block_request block_req;

   auto duration = std::chrono::system_clock::now().time_since_epoch();
   block_req.mutable_block()->mutable_header()->set_timestamp( std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count() );
   block_req.mutable_block()->mutable_header()->set_height( 1 );
   block_req.mutable_block()->mutable_header()->set_previous( util::converter::as< std::string >( koinos::crypto::multihash::zero( koinos::crypto::multicodec::sha2_256 ) ) );
   block_req.mutable_block()->mutable_header()->set_previous_state_merkle_root( _controller.get_head_info().head_state_merkle_root() );
   *block_req.mutable_block()->add_transactions() = trx1;
   *block_req.mutable_block()->add_transactions() = trx2;

   set_block_merkle_roots( *block_req.mutable_block(), koinos::crypto::multicodec::sha2_256 );
   block_req.mutable_block()->set_id( util::converter::as< std::string >( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, block_req.block().header() ) ) );
   sign_block( *block_req.mutable_block(), _block_signing_private_key );

   _controller.submit_block( block_req );

   BOOST_TEST_MESSAGE( "Verify mint did nothing" );

   koinos::contracts::token::balance_of_arguments bal_args;
   bal_args.set_owner( alice_address );

   koinos::rpc::chain::read_contract_request request;
   request.set_contract_id( op1->contract_id() );
   request.set_entry_point( 0x15619248 );
   request.set_args( bal_args.SerializeAsString() );

   auto response = _controller.read_contract( request );
   koinos::contracts::token::balance_of_result bal_ret;
   bal_ret.ParseFromString( response.result() );

   BOOST_REQUIRE_EQUAL( bal_ret.value(), 0 );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( receipt_test )
{ try {
   BOOST_TEST_MESSAGE( "Submit block with KOIN contract upload and attempted mint" );

   auto rc_limit1 = 10'000'000;
   auto rc_limit2 = 9'000'000;

   auto contract_private_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "contract"s ) );
   auto alice_private_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "alice"s ) );
   auto alice_address = alice_private_key.get_public_key().to_address_bytes();
   koinos::protocol::transaction trx1;

   // Upload the KOIN contract
   auto op1 = trx1.add_operations()->mutable_upload_contract();
   op1->set_contract_id( util::converter::as< std::string >( contract_private_key.get_public_key().to_address_bytes() ) );
   op1->set_bytecode( util::converter::as< std::string >( get_koin_wasm() ) );
   trx1.mutable_header()->set_rc_limit( rc_limit1 );
   set_transaction_merkle_roots( trx1, koinos::crypto::multicodec::sha2_256 );
   trx1.set_id( util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, trx1.header() ) ) );
   sign_transaction( trx1, contract_private_key );

   koinos::protocol::transaction trx2;

   koinos::contracts::token::mint_arguments mint_arg;
   mint_arg.set_to( alice_address );
   mint_arg.set_value( 100 );

   auto op2 = trx2.add_operations()->mutable_call_contract();
   op2->set_contract_id( op1->contract_id() );
   op2->set_entry_point( 0xc2f82bdc );
   op2->set_args( mint_arg.SerializeAsString() );
   trx2.mutable_header()->set_rc_limit( rc_limit2 );
   set_transaction_merkle_roots( trx2, koinos::crypto::multicodec::sha2_256 );
   trx2.set_id( util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, trx2.header() ) ) );
   sign_transaction( trx2, alice_private_key );

   koinos::rpc::chain::submit_block_request block_req;

   auto duration = std::chrono::system_clock::now().time_since_epoch();
   block_req.mutable_block()->mutable_header()->set_timestamp( std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count() );
   block_req.mutable_block()->mutable_header()->set_height( 1 );
   block_req.mutable_block()->mutable_header()->set_previous( util::converter::as< std::string >( koinos::crypto::multihash::zero( koinos::crypto::multicodec::sha2_256 ) ) );
   block_req.mutable_block()->mutable_header()->set_previous_state_merkle_root( _controller.get_head_info().head_state_merkle_root() );
   *block_req.mutable_block()->add_transactions() = trx1;
   *block_req.mutable_block()->add_transactions() = trx2;

   set_block_merkle_roots( *block_req.mutable_block(), koinos::crypto::multicodec::sha2_256 );
   block_req.mutable_block()->set_id( util::converter::as< std::string >( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, block_req.block().header() ) ) );
   sign_block( *block_req.mutable_block(), _block_signing_private_key );

   auto rld = _controller.get_resource_limits( koinos::rpc::chain::get_resource_limits_request() );

   koinos::rpc::chain::get_account_rc_request rc_req;

   rc_req.set_account( util::converter::as< std::string >( contract_private_key.get_public_key().to_address_bytes() ) );
   uint64_t max_payer1 = _controller.get_account_rc( rc_req ).rc();

   rc_req.set_account( util::converter::as< std::string >( alice_private_key.get_public_key().to_address_bytes() ) );
   uint64_t max_payer2 = _controller.get_account_rc( rc_req ).rc();

   auto block_resp = _controller.submit_block( block_req );

   BOOST_TEST_MESSAGE( "Check the resulting block receipt" );

   LOG(info) << block_resp;

   BOOST_REQUIRE_EQUAL( block_resp.receipt().id(), block_req.block().id() );
   BOOST_REQUIRE_EQUAL( block_resp.receipt().transaction_receipts_size(), 2 );
   BOOST_REQUIRE_EQUAL( block_resp.receipt().events_size(), 0 );
   BOOST_REQUIRE(
      block_resp.receipt().disk_storage_used() >=
      block_resp.receipt().transaction_receipts( 0 ).disk_storage_used() +
      block_resp.receipt().transaction_receipts( 1 ).disk_storage_used() );

   BOOST_REQUIRE(
      block_resp.receipt().network_bandwidth_used() >=
      block_resp.receipt().transaction_receipts( 0 ).network_bandwidth_used() +
      block_resp.receipt().transaction_receipts( 1 ).network_bandwidth_used() );

   BOOST_REQUIRE(
      block_resp.receipt().compute_bandwidth_used() >=
      block_resp.receipt().transaction_receipts( 0 ).compute_bandwidth_used() +
      block_resp.receipt().transaction_receipts( 1 ).compute_bandwidth_used() );

   uint64_t rc1 = 0;
   rc1 += block_resp.receipt().transaction_receipts( 0 ).disk_storage_used() * rld.resource_limit_data().disk_storage_cost();
   rc1 += block_resp.receipt().transaction_receipts( 0 ).compute_bandwidth_used() * rld.resource_limit_data().compute_bandwidth_cost();
   rc1 += block_resp.receipt().transaction_receipts( 0 ).network_bandwidth_used() * rld.resource_limit_data().network_bandwidth_cost();

   BOOST_REQUIRE_EQUAL( block_resp.receipt().transaction_receipts( 0 ).id(), trx1.id() );
   BOOST_REQUIRE_EQUAL( block_resp.receipt().transaction_receipts( 0 ).rc_limit(), rc_limit1 );
   BOOST_REQUIRE_EQUAL( block_resp.receipt().transaction_receipts( 0 ).rc_used(), rc1 );
   BOOST_REQUIRE_EQUAL( block_resp.receipt().transaction_receipts( 0 ).payer(), util::converter::as< std::string >( contract_private_key.get_public_key().to_address_bytes() ) );
   BOOST_REQUIRE_EQUAL( block_resp.receipt().transaction_receipts( 0 ).max_payer_rc(), max_payer1 );
   BOOST_REQUIRE_EQUAL( block_resp.receipt().transaction_receipts( 0 ).reverted(), false );
   BOOST_REQUIRE_EQUAL( block_resp.receipt().transaction_receipts( 0 ).events_size(), 0 );

   uint64_t rc2 = 0;
   rc2 += block_resp.receipt().transaction_receipts( 1 ).disk_storage_used() * rld.resource_limit_data().disk_storage_cost();
   rc2 += block_resp.receipt().transaction_receipts( 1 ).compute_bandwidth_used() * rld.resource_limit_data().compute_bandwidth_cost();
   rc2 += block_resp.receipt().transaction_receipts( 1 ).network_bandwidth_used() * rld.resource_limit_data().network_bandwidth_cost();

   BOOST_REQUIRE_EQUAL( block_resp.receipt().transaction_receipts( 1 ).id(), trx2.id() );
   BOOST_REQUIRE_EQUAL( block_resp.receipt().transaction_receipts( 1 ).rc_limit(), rc_limit2 );
   BOOST_REQUIRE_EQUAL( block_resp.receipt().transaction_receipts( 1 ).rc_used(), rc2 );
   BOOST_REQUIRE_EQUAL( block_resp.receipt().transaction_receipts( 1 ).payer(), util::converter::as< std::string >( alice_private_key.get_public_key().to_address_bytes() ) );
   BOOST_REQUIRE_EQUAL( block_resp.receipt().transaction_receipts( 1 ).max_payer_rc(), max_payer2 );
   BOOST_REQUIRE_EQUAL( block_resp.receipt().transaction_receipts( 1 ).reverted(), false );
   BOOST_REQUIRE_EQUAL( block_resp.receipt().transaction_receipts( 1 ).events_size(), 0 );

   LOG(info) << block_resp;

   auto rc_limit3 = 8'000'000;

   koinos::protocol::transaction trx3;

   koinos::contracts::token::transfer_arguments xfer_arg;
   xfer_arg.set_from( alice_address );
   xfer_arg.set_to( contract_private_key.get_public_key().to_address_bytes() );
   xfer_arg.set_value( 100 );

   auto op3 = trx3.add_operations()->mutable_call_contract();
   op3->set_contract_id( op1->contract_id() );
   op3->set_entry_point( 0x62efa292 );
   op3->set_args( xfer_arg.SerializeAsString() );
   trx3.mutable_header()->set_rc_limit( rc_limit3 );
   trx3.mutable_header()->set_nonce( 1 );
   set_transaction_merkle_roots( trx3, koinos::crypto::multicodec::sha2_256 );
   trx3.set_id( util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, trx2.header() ) ) );
   sign_transaction( trx3, alice_private_key );

   koinos::rpc::chain::submit_transaction_request tx_req;
   *tx_req.mutable_transaction() = trx3;

   rld = _controller.get_resource_limits( koinos::rpc::chain::get_resource_limits_request() );

   rc_req.set_account( util::converter::as< std::string >( alice_private_key.get_public_key().to_address_bytes() ) );

   uint64_t max_payer3 = _controller.get_account_rc( rc_req ).rc();

   auto tx_resp = _controller.submit_transaction( tx_req );

   uint64_t rc3 = 0;
   rc3 += tx_resp.receipt().disk_storage_used() * rld.resource_limit_data().disk_storage_cost();
   rc3 += tx_resp.receipt().compute_bandwidth_used() * rld.resource_limit_data().compute_bandwidth_cost();
   rc3 += tx_resp.receipt().network_bandwidth_used() * rld.resource_limit_data().network_bandwidth_cost();

   BOOST_TEST_MESSAGE( "Check the resulting transaction receipt" );

   BOOST_REQUIRE_EQUAL( tx_resp.receipt().id(), trx3.id() );
   BOOST_REQUIRE_EQUAL( tx_resp.receipt().rc_limit(), rc_limit3 );
   BOOST_REQUIRE_EQUAL( tx_resp.receipt().rc_used(), rc3 );
   BOOST_REQUIRE_EQUAL( tx_resp.receipt().payer(), util::converter::as< std::string >( alice_private_key.get_public_key().to_address_bytes() ) );
   BOOST_REQUIRE_EQUAL( tx_resp.receipt().max_payer_rc(), max_payer3 );
   BOOST_REQUIRE_EQUAL( tx_resp.receipt().reverted(), false );
   BOOST_REQUIRE_EQUAL( tx_resp.receipt().events_size(), 0 );

   LOG(info) << tx_resp.receipt();

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_SUITE_END()
