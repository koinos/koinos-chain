#include <boost/test/unit_test.hpp>

#include <koinos/crypto/multihash.hpp>
#include <koinos/log/log.hpp>
#include <koinos/pack/rt/json.hpp>
#include <koinos/statedb/statedb.hpp>

#include <mira/database_configuration.hpp>

using namespace koinos::crypto;
using namespace koinos::statedb;

struct test_block
{
   multihash_type previous;
   uint64_t       block_num = 0;
   uint64_t       nonce = 0;

   void get_id( multihash_type& mh ) const;
};

KOINOS_REFLECT( test_block, (previous)(block_num)(nonce) )

void test_block::get_id( multihash_type& mh )const
{
   return hash( mh, CRYPTO_SHA2_256_ID, *this );
}

struct statedb_fixture
{
   statedb_fixture()
   {
      temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
      boost::filesystem::create_directory( temp );
      boost::any cfg = mira::utilities::default_database_configuration();

      db.open( temp, cfg );
   }

   ~statedb_fixture()
   {
      db.close();
      boost::filesystem::remove_all( temp );
   }

   state_db db;
   boost::filesystem::path temp;
};

BOOST_FIXTURE_TEST_SUITE( statedb_tests, statedb_fixture )

BOOST_AUTO_TEST_CASE( fork_tests )
{ try {
   BOOST_TEST_MESSAGE( "Basic fork tests on statedb" );
   multihash_type id, prev_id, block_1000_id;
   test_block b;

   prev_id = db.get_root()->id();

   for( uint64_t i = 1; i <= 2000; ++i )
   {
      b.previous = prev_id;
      b.block_num = i;
      b.get_id( id );

      auto new_block = db.create_writable_node( prev_id, id );
      BOOST_CHECK_EQUAL( b.block_num, new_block->revision() );
      db.finalize_node( id );

      prev_id = id;

      if( i == 1000 ) block_1000_id = id;
   }

   BOOST_REQUIRE( db.get_root()->id() == zero_hash( CRYPTO_SHA2_256_ID ) );
   BOOST_REQUIRE( db.get_root()->revision() == 0 );

   BOOST_REQUIRE( db.get_head()->id() == prev_id );
   BOOST_REQUIRE( db.get_head()->revision() == 2000 );

   BOOST_REQUIRE( db.get_node( block_1000_id )->id() == block_1000_id );
   BOOST_REQUIRE( db.get_node( block_1000_id )->revision() == 1000 );

   BOOST_TEST_MESSAGE( "Test commit" );
   db.commit_node( block_1000_id );
   BOOST_REQUIRE( db.get_root()->id() == block_1000_id );
   BOOST_REQUIRE( db.get_root()->revision() == 1000 );

   multihash_type block_2000_id = id;

   BOOST_TEST_MESSAGE( "Test discard" );
   b.previous = db.get_head()->id();
   b.block_num = db.get_head()->revision() + 1;
   b.get_id( id );
   db.create_writable_node( b.previous, id );
   auto new_block = db.get_node( id );
   BOOST_REQUIRE( new_block );

   db.discard_node( id );

   BOOST_REQUIRE( db.get_head()->id() == prev_id );
   BOOST_REQUIRE( db.get_head()->revision() == 2000 );

   // Shared ptr should still exist, but not be returned with get_node
   BOOST_REQUIRE( new_block );
   BOOST_REQUIRE( !db.get_node( id ) );
   new_block.reset();

   // Cannot discard head
   BOOST_REQUIRE_THROW( db.discard_node( prev_id ), cannot_discard );

   BOOST_TEST_MESSAGE( "Check duplicate node creation" );
   BOOST_REQUIRE( !db.create_writable_node( db.get_head()->parent_id(), db.get_head()->id() ) );

   BOOST_TEST_MESSAGE( "Check failed linking" );
   multihash_type zero;
   zero_hash( zero, CRYPTO_SHA2_256_ID );
   BOOST_REQUIRE( !db.create_writable_node( zero, id ) );

   multihash_type head_id = db.get_head()->id();
   uint64_t head_rev = db.get_head()->revision();

   BOOST_TEST_MESSAGE( "Test minority fork" );
   auto fork_node = db.get_node_at_revision( 1995 );
   prev_id = fork_node->id();
   b.nonce = 1;

   for( uint64_t i = 1; i <= 5; ++i )
   {
      b.previous = prev_id;
      b.block_num = fork_node->revision() + i;
      b.get_id( id );

      auto new_block = db.create_writable_node( prev_id, id );
      BOOST_CHECK_EQUAL( b.block_num, new_block->revision() );
      db.finalize_node( id );

      BOOST_CHECK( db.get_head()->id() == head_id );
      BOOST_CHECK( db.get_head()->revision() == head_rev );

      prev_id = id;
   }

   b.previous = prev_id;
   b.block_num = head_rev + 1;
   b.get_id( id );

   // When this node finalizes, it will be the longest path and should become head
   new_block = db.create_writable_node( prev_id, id );
   BOOST_CHECK_EQUAL( b.block_num, new_block->revision() );

   BOOST_CHECK( db.get_head()->id() == head_id );
   BOOST_CHECK( db.get_head()->revision() == head_rev );

   db.finalize_node( id );

   BOOST_CHECK( db.get_head()->id() == id );
   BOOST_CHECK( db.get_head()->revision() == b.block_num );

} catch( const koinos::exception::koinos_exception& e ) { LOG(info) << e.to_string(); throw e; } }


BOOST_AUTO_TEST_SUITE_END()