#include <boost/test/unit_test.hpp>

#include <koinos/bigint.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/log.hpp>
#include <koinos/exception.hpp>
#include <koinos/state_db/backends/map/map_backend.hpp>
#include <koinos/state_db/backends/rocksdb/rocksdb_backend.hpp>
#include <koinos/state_db/detail/merge_iterator.hpp>
#include <koinos/state_db/detail/state_delta.hpp>
#include <koinos/state_db/state_db.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/random.hpp>


#include <deque>
#include <iostream>
#include <filesystem>

using namespace koinos;
using namespace koinos::state_db;
using state_db::detail::merge_state;
using state_db::detail::state_delta;
using namespace std::string_literals;

struct test_block
{
   std::string       previous;
   uint64_t          height = 0;
   uint64_t          nonce = 0;

   crypto::multihash get_id() const;
};

crypto::multihash test_block::get_id() const
{
   return crypto::hash( crypto::multicodec::sha2_256, util::converter::to< crypto::multihash >( previous ), height, nonce );
}

struct state_db_fixture
{
   state_db_fixture()
   {
      initialize_logging( "koinos_test", {}, "info" );

      temp = std::filesystem::temp_directory_path() / util::random_alphanumeric( 8 );
      std::filesystem::create_directory( temp );

      db.open( temp );
   }

   ~state_db_fixture()
   {
      boost::log::core::get()->remove_all_sinks();
      db.close();
      std::filesystem::remove_all( temp );
   }

   database db;
   std::filesystem::path temp;
};

BOOST_FIXTURE_TEST_SUITE( state_db_tests, state_db_fixture )

BOOST_AUTO_TEST_CASE( basic_test )
{ try {
   BOOST_TEST_MESSAGE( "Creating object" );
   object_space space;
   std::string a_key = "a";
   std::string a_val = "alice";

   crypto::multihash state_id = crypto::hash( crypto::multicodec::sha2_256, 1 );
   auto state_1 = db.create_writable_node( db.get_head()->id(), state_id );
   BOOST_CHECK_EQUAL( state_1->put_object( space, a_key, &a_val ), a_val.size() );

   // Object should not exist on older state node
   BOOST_CHECK_EQUAL( db.get_root()->get_object( space, a_key ), nullptr );

   auto ptr = state_1->get_object( space, a_key );
   BOOST_REQUIRE( ptr );
   BOOST_CHECK_EQUAL( *ptr, a_val );

   BOOST_TEST_MESSAGE( "Modifying object" );

   a_val = "alicia";
   BOOST_CHECK_EQUAL( state_1->put_object( space, a_key, &a_val ), 1 );

   ptr = state_1->get_object( space, a_key );
   BOOST_REQUIRE( ptr );
   BOOST_CHECK_EQUAL( *ptr, a_val );

   state_id = crypto::hash( crypto::multicodec::sha2_256, 2 );
   auto state_2 = db.create_writable_node( state_1->id(), state_id );
   BOOST_CHECK( !state_2 );

   db.finalize_node( state_1->id() );

   BOOST_REQUIRE_THROW( state_1->put_object( space, a_key, &a_val ), node_finalized );

   state_2 = db.create_writable_node( state_1->id(), state_id );
   a_val = "alex";
   BOOST_CHECK_EQUAL( state_2->put_object( space, a_key, &a_val ), -2 );

   ptr = state_2->get_object( space, a_key );
   BOOST_REQUIRE( ptr );
   BOOST_CHECK_EQUAL( *ptr, a_val );

   ptr = state_1->get_object( space, a_key );
   BOOST_REQUIRE( ptr );
   BOOST_CHECK_EQUAL( *ptr, "alicia" );

   BOOST_TEST_MESSAGE( "Erasing object" );
   BOOST_CHECK_EQUAL( state_2->put_object( space, a_key, nullptr ), -1 * a_val.size() );

   BOOST_CHECK( !state_2->get_object( space, a_key ) );

   db.discard_node( state_2->id() );
   state_2 = db.get_node( state_2->id() );
   BOOST_CHECK( !state_2 );

   ptr = state_1->get_object( space, a_key );
   BOOST_REQUIRE( ptr );
   BOOST_CHECK_EQUAL( *ptr, "alicia" );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( fork_tests )
{ try {
   BOOST_TEST_MESSAGE( "Basic fork tests on state_db" );
   crypto::multihash id, prev_id, block_1000_id;
   test_block b;

   prev_id = db.get_root()->id();

   for( uint64_t i = 1; i <= 2000; ++i )
   {
      b.previous = util::converter::as< std::string >( prev_id );
      b.height = i;
      id = b.get_id();

      auto new_block = db.create_writable_node( prev_id, id );
      BOOST_CHECK_EQUAL( b.height, new_block->revision() );
      db.finalize_node( id );

      prev_id = id;

      if( i == 1000 ) block_1000_id = id;
   }

   BOOST_REQUIRE( db.get_root()->id() == crypto::multihash::zero( crypto::multicodec::sha2_256 ) );
   BOOST_REQUIRE( db.get_root()->revision() == 0 );

   BOOST_REQUIRE( db.get_head()->id() == prev_id );
   BOOST_REQUIRE( db.get_head()->revision() == 2000 );

   BOOST_REQUIRE( db.get_node( block_1000_id )->id() == block_1000_id );
   BOOST_REQUIRE( db.get_node( block_1000_id )->revision() == 1000 );

   auto fork_heads = db.get_fork_heads();
   BOOST_REQUIRE_EQUAL( fork_heads.size(), 1 );
   BOOST_REQUIRE( fork_heads[0]->id() == db.get_head()->id() );

   BOOST_TEST_MESSAGE( "Test commit" );
   db.commit_node( block_1000_id );
   BOOST_REQUIRE( db.get_root()->id() == block_1000_id );
   BOOST_REQUIRE( db.get_root()->revision() == 1000 );

   fork_heads = db.get_fork_heads();
   BOOST_REQUIRE_EQUAL( fork_heads.size(), 1 );
   BOOST_REQUIRE( fork_heads[0]->id() == db.get_head()->id() );

   crypto::multihash block_2000_id = id;

   BOOST_TEST_MESSAGE( "Test discard" );
   b.previous = util::converter::as< std::string >( db.get_head()->id() );
   b.height = db.get_head()->revision() + 1;
   id = b.get_id();
   db.create_writable_node( util::converter::to< crypto::multihash >( b.previous ), id );
   auto new_block = db.get_node( id );
   BOOST_REQUIRE( new_block );

   fork_heads = db.get_fork_heads();
   BOOST_REQUIRE_EQUAL( fork_heads.size(), 1 );
   BOOST_REQUIRE( fork_heads[0]->id() == prev_id );

   db.discard_node( id );

   BOOST_REQUIRE( db.get_head()->id() == prev_id );
   BOOST_REQUIRE( db.get_head()->revision() == 2000 );

   fork_heads = db.get_fork_heads();
   BOOST_REQUIRE_EQUAL( fork_heads.size(), 1 );
   BOOST_REQUIRE( fork_heads[0]->id() == prev_id );

   // Shared ptr should still exist, but not be returned with get_node
   BOOST_REQUIRE( new_block );
   BOOST_REQUIRE( !db.get_node( id ) );
   new_block.reset();

   // Cannot discard head
   BOOST_REQUIRE_THROW( db.discard_node( prev_id ), cannot_discard );

   BOOST_TEST_MESSAGE( "Check duplicate node creation" );
   BOOST_REQUIRE( !db.create_writable_node( db.get_head()->parent_id(), db.get_head()->id() ) );

   BOOST_TEST_MESSAGE( "Check failed linking" );
   crypto::multihash zero = crypto::multihash::zero( crypto::multicodec::sha2_256 );
   BOOST_REQUIRE( !db.create_writable_node( zero, id ) );

   crypto::multihash head_id = db.get_head()->id();
   uint64_t head_rev = db.get_head()->revision();

   BOOST_TEST_MESSAGE( "Test minority fork" );
   auto fork_node = db.get_node_at_revision( 1995 );
   prev_id = fork_node->id();
   b.nonce = 1;

   auto old_block_1996_id = db.get_node_at_revision( 1996 )->id();
   auto old_block_1997_id = db.get_node_at_revision( 1997 )->id();

   for ( uint64_t i = 1; i <= 5; ++i )
   {
      b.previous = util::converter::as< std::string >( prev_id );
      b.height = fork_node->revision() + i;
      id = b.get_id();

      auto new_block = db.create_writable_node( prev_id, id );
      BOOST_CHECK_EQUAL( b.height, new_block->revision() );
      db.finalize_node( id );

      BOOST_CHECK( db.get_head()->id() == head_id );
      BOOST_CHECK( db.get_head()->revision() == head_rev );

      prev_id = id;
   }

   fork_heads = db.get_fork_heads();
   BOOST_REQUIRE_EQUAL( fork_heads.size(), 2 );
   BOOST_REQUIRE( ( fork_heads[0]->id() == db.get_head()->id() && fork_heads[1]->id() == id ) ||
                  ( fork_heads[1]->id() == db.get_head()->id() && fork_heads[0]->id() == id ) );
   auto old_head_id = db.get_head()->id();

   b.previous = util::converter::as< std::string >( prev_id );
   b.height = head_rev + 1;
   id = b.get_id();

   // When this node finalizes, it will be the longest path and should become head
   new_block = db.create_writable_node( prev_id, id );
   BOOST_CHECK_EQUAL( b.height, new_block->revision() );

   BOOST_CHECK( db.get_head()->id() == head_id );
   BOOST_CHECK( db.get_head()->revision() == head_rev );

   db.finalize_node( id );

   fork_heads = db.get_fork_heads();
   BOOST_REQUIRE_EQUAL( fork_heads.size(), 2 );
   BOOST_REQUIRE( ( fork_heads[0]->id() == id && fork_heads[1]->id() == old_head_id ) ||
                  ( fork_heads[1]->id() == id && fork_heads[0]->id() == old_head_id ) );

   BOOST_CHECK( db.get_head()->id() == id );
   BOOST_CHECK( db.get_head()->revision() == b.height );

   db.discard_node( old_block_1997_id );
   fork_heads = db.get_fork_heads();
   BOOST_REQUIRE_EQUAL( fork_heads.size(), 2 );
   BOOST_REQUIRE( ( fork_heads[0]->id() == id && fork_heads[1]->id() == old_block_1996_id ) ||
                  ( fork_heads[1]->id() == id && fork_heads[0]->id() == old_block_1996_id ) );

   db.discard_node( old_block_1996_id );
   fork_heads = db.get_fork_heads();
   BOOST_REQUIRE_EQUAL( fork_heads.size(), 1 );
   BOOST_REQUIRE( fork_heads[0]->id() == id );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( merge_iterator )
{ try {
   std::filesystem::path temp = std::filesystem::temp_directory_path() / koinos::util::random_alphanumeric( 8 );
   std::filesystem::create_directory( temp );

   using state_delta_ptr = std::shared_ptr< state_delta >;
   std::deque< state_delta_ptr > delta_queue;
   delta_queue.emplace_back( std::make_shared< state_delta >( temp ) );

   // alice: 1
   // bob: 2
   // charlie: 3
   delta_queue.back()->put( "alice", "1" );
   delta_queue.back()->put( "bob", "2" );
   delta_queue.back()->put( "charlie", "3" );

   {
      merge_state m_state( delta_queue.back() );
      auto itr = m_state.begin();

      BOOST_REQUIRE( itr != m_state.end() );
      BOOST_CHECK_EQUAL( itr.key(), "alice" );
      BOOST_CHECK_EQUAL( *itr, "1" );
      ++itr;
      BOOST_CHECK_EQUAL( itr.key(), "bob" );
      BOOST_CHECK_EQUAL( *itr, "2" );
      ++itr;
      BOOST_CHECK_EQUAL( itr.key(), "charlie" );
      BOOST_CHECK_EQUAL( *itr, "3" );
      ++itr;
      BOOST_REQUIRE( itr == m_state.end() );
      BOOST_CHECK_THROW( *itr, koinos::exception );
      BOOST_CHECK_THROW( ++itr, koinos::exception );
      BOOST_CHECK_THROW( itr.key(), koinos::exception );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "charlie" );
      BOOST_CHECK_EQUAL( *itr, "3" );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "bob" );
      BOOST_CHECK_EQUAL( *itr, "2" );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "alice" );
      BOOST_CHECK_EQUAL( *itr, "1" );
   }


   // alice: 4
   // bob: 5
   // charlie: 3 (not changed)
   delta_queue.emplace_back( std::make_shared< state_delta >( delta_queue.back(), delta_queue.back()->id() ) );
   delta_queue.back()->put( "alice", "4" );
   delta_queue.back()->put( "bob", "5" );

   {
      merge_state m_state( delta_queue.back() );
      auto itr = m_state.begin();

      BOOST_REQUIRE( itr != m_state.end() );
      BOOST_CHECK_EQUAL( itr.key(), "alice" );
      BOOST_CHECK_EQUAL( *itr, "4" );
      ++itr;
      BOOST_CHECK_EQUAL( itr.key(), "bob" );
      BOOST_CHECK_EQUAL( *itr, "5" );
      ++itr;
      BOOST_CHECK_EQUAL( itr.key(), "charlie" );
      BOOST_CHECK_EQUAL( *itr, "3" );
      ++itr;
      BOOST_REQUIRE( itr == m_state.end() );
      BOOST_CHECK_THROW( *itr, koinos::exception );
      BOOST_CHECK_THROW( ++itr, koinos::exception );
      BOOST_CHECK_THROW( itr.key(), koinos::exception );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "charlie" );
      BOOST_CHECK_EQUAL( *itr, "3" );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "bob" );
      BOOST_CHECK_EQUAL( *itr, "5" );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "alice" );
      BOOST_CHECK_EQUAL( *itr, "4" );
   }

   // alice: 4 (not changed)
   // bob: 6
   // charlie: 3 (not changed)
   delta_queue.emplace_back( std::make_shared< state_delta >( delta_queue.back(), delta_queue.back()->id() ) );
   delta_queue.back()->put( "bob", "6" );

   {
      merge_state m_state( delta_queue.back() );
      auto itr = m_state.begin();

      BOOST_REQUIRE( itr != m_state.end() );
      BOOST_CHECK_EQUAL( itr.key(), "alice" );
      BOOST_CHECK_EQUAL( *itr, "4" );
      ++itr;
      BOOST_CHECK_EQUAL( itr.key(), "bob" );
      BOOST_CHECK_EQUAL( *itr, "6" );
      ++itr;
      BOOST_CHECK_EQUAL( itr.key(), "charlie" );
      BOOST_CHECK_EQUAL( *itr, "3" );
      ++itr;
      BOOST_REQUIRE( itr == m_state.end() );
      BOOST_CHECK_THROW( *itr, koinos::exception );
      BOOST_CHECK_THROW( ++itr, koinos::exception );
      BOOST_CHECK_THROW( itr.key(), koinos::exception );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "charlie" );
      BOOST_CHECK_EQUAL( *itr, "3" );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "bob" );
      BOOST_CHECK_EQUAL( *itr, "6" );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "alice" );
      BOOST_CHECK_EQUAL( *itr, "4" );
   }

   // alice: (removed)
   // bob: 6 (not changed)
   // charlie: 3 (not changed)
   delta_queue.emplace_back( std::make_shared< state_delta >( delta_queue.back(), delta_queue.back()->id() ) );
   delta_queue.back()->erase( "alice" );

   {
      merge_state m_state( delta_queue.back() );
      auto itr = m_state.begin();

      BOOST_REQUIRE( itr != m_state.end() );
      BOOST_CHECK_EQUAL( itr.key(), "bob" );
      BOOST_CHECK_EQUAL( *itr, "6" );
      ++itr;
      BOOST_CHECK_EQUAL( itr.key(), "charlie" );
      BOOST_CHECK_EQUAL( *itr, "3" );
      ++itr;
      BOOST_REQUIRE( itr == m_state.end() );
      BOOST_CHECK_THROW( *itr, koinos::exception );
      BOOST_CHECK_THROW( ++itr, koinos::exception );
      BOOST_CHECK_THROW( itr.key(), koinos::exception );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "charlie" );
      BOOST_CHECK_EQUAL( *itr, "3" );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "bob" );
      BOOST_CHECK_EQUAL( *itr, "6" );
   }

   // alice: 4 (restored)
   // bob: 6 (not changed)
   // charlie: 3 (not changed)
   delta_queue.emplace_back( std::make_shared< state_delta >( delta_queue.back(), delta_queue.back()->id() ) );
   delta_queue.back()->put( "alice", "4" );

   {
      merge_state m_state( delta_queue.back() );
      auto itr = m_state.begin();

      BOOST_REQUIRE( itr != m_state.end() );
      BOOST_CHECK_EQUAL( itr.key(), "alice" );
      BOOST_CHECK_EQUAL( *itr, "4" );
      ++itr;
      BOOST_CHECK_EQUAL( itr.key(), "bob" );
      BOOST_CHECK_EQUAL( *itr, "6" );
      ++itr;
      BOOST_CHECK_EQUAL( itr.key(), "charlie" );
      BOOST_CHECK_EQUAL( *itr, "3" );
      ++itr;
      BOOST_REQUIRE( itr == m_state.end() );
      BOOST_CHECK_THROW( *itr, koinos::exception );
      BOOST_CHECK_THROW( ++itr, koinos::exception );
      BOOST_CHECK_THROW( itr.key(), koinos::exception );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "charlie" );
      BOOST_CHECK_EQUAL( *itr, "3" );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "bob" );
      BOOST_CHECK_EQUAL( *itr, "6" );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "alice" );
      BOOST_CHECK_EQUAL( *itr, "4" );
   }

   delta_queue.pop_front();
   delta_queue.pop_front();
   delta_queue.front()->commit();

   {
      merge_state m_state( delta_queue.back() );
      auto itr = m_state.begin();

      BOOST_REQUIRE( itr != m_state.end() );
      BOOST_CHECK_EQUAL( itr.key(), "alice" );
      BOOST_CHECK_EQUAL( *itr, "4" );
      ++itr;
      BOOST_CHECK_EQUAL( itr.key(), "bob" );
      BOOST_CHECK_EQUAL( *itr, "6" );
      ++itr;
      BOOST_CHECK_EQUAL( itr.key(), "charlie" );
      BOOST_CHECK_EQUAL( *itr, "3" );
      ++itr;
      BOOST_REQUIRE( itr == m_state.end() );
      BOOST_CHECK_THROW( *itr, koinos::exception );
      BOOST_CHECK_THROW( ++itr, koinos::exception );
      BOOST_CHECK_THROW( itr.key(), koinos::exception );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "charlie" );
      BOOST_CHECK_EQUAL( *itr, "3" );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "bob" );
      BOOST_CHECK_EQUAL( *itr, "6" );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "alice" );
      BOOST_CHECK_EQUAL( *itr, "4" );
   }

   while( delta_queue.size() > 1 )
   {
      delta_queue.pop_front();
      delta_queue.front()->commit();

      merge_state m_state( delta_queue.back() );
      auto itr = m_state.begin();

      BOOST_REQUIRE( itr != m_state.end() );
      BOOST_CHECK_EQUAL( itr.key(), "alice" );
      BOOST_CHECK_EQUAL( *itr, "4" );
      ++itr;
      BOOST_CHECK_EQUAL( itr.key(), "bob" );
      BOOST_CHECK_EQUAL( *itr, "6" );
      ++itr;
      BOOST_CHECK_EQUAL( itr.key(), "charlie" );
      BOOST_CHECK_EQUAL( *itr, "3" );
      ++itr;
      BOOST_REQUIRE( itr == m_state.end() );
      BOOST_CHECK_THROW( *itr, koinos::exception );
      BOOST_CHECK_THROW( ++itr, koinos::exception );
      BOOST_CHECK_THROW( itr.key(), koinos::exception );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "charlie" );
      BOOST_CHECK_EQUAL( *itr, "3" );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "bob" );
      BOOST_CHECK_EQUAL( *itr, "6" );
      --itr;
      BOOST_CHECK_EQUAL( itr.key(), "alice" );
      BOOST_CHECK_EQUAL( *itr, "4" );
   }
} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( reset_test )
{ try {
   BOOST_TEST_MESSAGE( "Creating object on transient state node" );

   crypto::multihash state_id = crypto::hash( crypto::multicodec::sha2_256, 1 );
   auto state_1 = db.create_writable_node( db.get_head()->id(), state_id );
   object_space space;
   std::string a_key = "a";
   std::string a_val = "alice";

   BOOST_CHECK_EQUAL( state_1->put_object( space, a_key, &a_val ), a_val.size() );
   db.finalize_node( state_1->id() );

   auto val_ptr = db.get_head()->get_object( space, a_key );
   BOOST_REQUIRE( val_ptr );
   BOOST_CHECK_EQUAL( *val_ptr, a_val );

   BOOST_TEST_MESSAGE( "Closing and opening database" );
   state_1.reset();
   db.close();

   BOOST_CHECK_THROW( db.reset(), koinos::exception );
   BOOST_CHECK_THROW( db.get_node_at_revision( 1 ), koinos::exception );
   BOOST_CHECK_THROW( db.get_node_at_revision( 1, crypto::hash( crypto::multicodec::sha2_256, 1 ) ), koinos::exception );
   BOOST_CHECK_THROW( db.get_node( crypto::hash( crypto::multicodec::sha2_256, 1 ) ), koinos::exception );
   BOOST_CHECK_THROW( db.create_writable_node( crypto::multihash::zero( crypto::multicodec::sha2_256 ), crypto::hash( crypto::multicodec::sha2_256, 1 ) ), koinos::exception );
   BOOST_CHECK_THROW( db.finalize_node( crypto::hash( crypto::multicodec::sha2_256, 1 ) ), koinos::exception );
   BOOST_CHECK_THROW( db.discard_node( crypto::hash( crypto::multicodec::sha2_256, 1 ) ), koinos::exception );
   BOOST_CHECK_THROW( db.commit_node( crypto::hash( crypto::multicodec::sha2_256, 1 ) ), koinos::exception );
   BOOST_CHECK_THROW( db.get_head(), koinos::exception );
   BOOST_CHECK_THROW( db.get_fork_heads(), koinos::exception );
   BOOST_CHECK_THROW( db.get_root(), koinos::exception );

   db.open( temp );

   // Object should not exist on persistent database (state node was not committed)
   BOOST_CHECK( !db.get_head()->get_object( space, a_key ) );
   BOOST_CHECK( db.get_head()->id() == crypto::multihash::zero( crypto::multicodec::sha2_256 ) );
   BOOST_CHECK( db.get_head()->revision() == 0 );

   BOOST_TEST_MESSAGE( "Creating object on committed state node" );

   state_1 = db.create_writable_node( db.get_head()->id(), state_id );
   BOOST_CHECK_EQUAL( state_1->put_object( space, a_key, &a_val ), a_val.size() );
   db.finalize_node( state_1->id() );
   db.commit_node( state_1->id() );

   val_ptr = db.get_head()->get_object( space, a_key );
   BOOST_REQUIRE( val_ptr );
   BOOST_CHECK_EQUAL( *val_ptr, a_val );
   BOOST_CHECK( db.get_head()->id() == crypto::hash( crypto::multicodec::sha2_256, 1 ) );

   BOOST_TEST_MESSAGE( "Closing and opening database" );
   state_1.reset();
   db.close();
   db.open( temp );

   // State node was committed and should exist on open
   val_ptr = db.get_head()->get_object( space, a_key );
   BOOST_REQUIRE( val_ptr );
   BOOST_CHECK_EQUAL( *val_ptr, a_val );
   BOOST_CHECK( db.get_head()->id() == crypto::hash( crypto::multicodec::sha2_256, 1 ) );
   BOOST_CHECK( db.get_head()->revision() == 1 );

   BOOST_TEST_MESSAGE( "Resetting database" );
   db.reset();

   // Object should not exist on reset db
   BOOST_CHECK( !db.get_head()->get_object( space, a_key ) );
   BOOST_CHECK( db.get_head()->id() == crypto::multihash::zero( crypto::multicodec::sha2_256 ) );
   BOOST_CHECK( db.get_head()->revision() == 0 );
} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( anonymous_node_test )
{ try {
   BOOST_TEST_MESSAGE( "Creating object" );
   object_space space;

   crypto::multihash state_id = crypto::hash( crypto::multicodec::sha2_256, 1 );
   auto state_1 = db.create_writable_node( db.get_head()->id(), state_id );
   std::string a_key = "a";
   std::string a_val = "alice";

   BOOST_CHECK( state_1->put_object( space, a_key, &a_val ) == a_val.size() );

   auto ptr = state_1->get_object( space, a_key );
   BOOST_REQUIRE( ptr );
   BOOST_CHECK_EQUAL( *ptr, a_val );

   {
      BOOST_TEST_MESSAGE( "Creating anonymous state node" );
      auto anon_state = state_1->create_anonymous_node();

      BOOST_REQUIRE( anon_state->id() == state_1->id() );
      BOOST_REQUIRE( anon_state->revision() == state_1->revision() );
      BOOST_REQUIRE( anon_state->parent_id() == state_1->parent_id() );

      BOOST_TEST_MESSAGE( "Modifying object" );
      a_val = "alicia";

      BOOST_CHECK( anon_state->put_object( space, a_key, &a_val ) == 1 );

      ptr = anon_state->get_object( space, a_key );
      BOOST_REQUIRE( ptr );
      BOOST_CHECK_EQUAL( *ptr, a_val );

      ptr = state_1->get_object( space, a_key );
      BOOST_REQUIRE( ptr );
      BOOST_CHECK_EQUAL( *ptr, "alice" );

      BOOST_TEST_MESSAGE( "Deleting anonymous node" );
   }

   {
      BOOST_TEST_MESSAGE( "Creating anonymous state node" );
      auto anon_state = state_1->create_anonymous_node();

      BOOST_TEST_MESSAGE( "Modifying object" );

      BOOST_CHECK( anon_state->put_object( space, a_key, &a_val ) == 1 );

      ptr = anon_state->get_object( space, a_key );
      BOOST_REQUIRE( ptr );
      BOOST_CHECK_EQUAL( *ptr, a_val );

      ptr = state_1->get_object( space, a_key );
      BOOST_REQUIRE( ptr );
      BOOST_CHECK_EQUAL( *ptr, "alice" );

      BOOST_TEST_MESSAGE( "Committing anonymous node" );
      anon_state->commit();

      ptr = state_1->get_object( space, a_key );
      BOOST_REQUIRE( ptr );
      BOOST_CHECK_EQUAL( *ptr, a_val );
   }

   ptr = state_1->get_object( space, a_key );
   BOOST_REQUIRE( ptr );
   BOOST_CHECK_EQUAL( *ptr, a_val );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( rocksdb_backend_test )
{ try {
   koinos::state_db::backends::rocksdb::rocksdb_backend backend;
   auto temp = std::filesystem::temp_directory_path() / util::random_alphanumeric( 8 );

   BOOST_REQUIRE_THROW( backend.open( temp ), koinos::exception );

   BOOST_CHECK_THROW( backend.begin(), koinos::exception );
   BOOST_CHECK_THROW( backend.end(), koinos::exception );
   BOOST_CHECK_THROW( backend.put( "foo", "bar" ), koinos::exception );
   BOOST_CHECK_THROW( backend.get( "foo" ), koinos::exception );
   BOOST_CHECK_THROW( backend.erase( "foo" ), koinos::exception );
   BOOST_CHECK_THROW( backend.clear(), koinos::exception );
   BOOST_CHECK_THROW( backend.size(), koinos::exception );
   BOOST_CHECK_THROW( backend.empty(), koinos::exception );
   BOOST_CHECK_THROW( backend.find( "foo" ), koinos::exception );
   BOOST_CHECK_THROW( backend.lower_bound( "foo" ), koinos::exception );
   BOOST_CHECK_THROW( backend.flush(), koinos::exception );
   BOOST_CHECK_THROW( backend.revision(), koinos::exception );
   BOOST_CHECK_THROW( backend.set_revision( 1 ), koinos::exception );
   BOOST_CHECK_THROW( backend.id(), koinos::exception );
   BOOST_CHECK_THROW( backend.set_id( koinos::crypto::multihash::zero( koinos::crypto::multicodec::sha2_256 ) ), koinos::exception );

   std::filesystem::create_directory( temp );
   backend.open( temp );

   auto itr = backend.begin();
   BOOST_CHECK( itr == backend.end() );

   backend.put( "foo", "bar" );
   itr = backend.begin();
   BOOST_CHECK( itr != backend.end() );
   BOOST_CHECK( *itr == "bar" );

   backend.put( "alice", "bob" );

   itr = backend.begin();
   BOOST_CHECK( itr != backend.end() );
   BOOST_CHECK( *itr == "bob" );

   ++itr;
   BOOST_CHECK( *itr == "bar" );

   ++itr;
   BOOST_CHECK( itr == backend.end() );

   --itr;
   BOOST_CHECK( itr != backend.end() );
   BOOST_CHECK( *itr == "bar" );

   itr = backend.lower_bound( "charlie" );
   BOOST_CHECK( itr != backend.end() );
   BOOST_CHECK( *itr == "bar" );

   itr = backend.lower_bound( "foo" );
   BOOST_CHECK( itr != backend.end() );
   BOOST_CHECK( *itr == "bar" );

   backend.put( "foo", "blob" );
   itr = backend.find( "foo" );
   BOOST_CHECK( itr != backend.end() );
   BOOST_CHECK( *itr == "blob" );

   --itr;
   BOOST_CHECK( itr != backend.end() );
   BOOST_CHECK( *itr == "bob" );

   backend.erase( "foo" );

   itr = backend.begin();
   BOOST_CHECK( itr != backend.end() );
   BOOST_CHECK( *itr == "bob" );

   itr = backend.find( "foo" );
   BOOST_CHECK( itr == backend.end() );

   backend.erase( "foo" );

   backend.erase( "alice" );
   itr = backend.end();
   BOOST_CHECK( itr == backend.end() );

   std::filesystem::remove_all( temp );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( rocksdb_object_cache_test )
{ try {
   std::size_t cache_size = 1024;
   koinos::state_db::backends::rocksdb::object_cache cache( cache_size );

   std::string a_key = "a";
   std::string a_val = "alice";

   BOOST_CHECK( !cache.get( a_key ) );
   BOOST_CHECK( cache.put( a_key, a_val ) );

   auto val_ptr = cache.get( a_key );
   BOOST_REQUIRE( val_ptr );
   BOOST_CHECK_EQUAL( *val_ptr, a_val );

   std::string b_key = "b";
   std::string b_val = "bob";

   cache.put( b_key, b_val );
   val_ptr = cache.get( b_key );
   BOOST_REQUIRE( val_ptr );
   BOOST_CHECK_EQUAL( *val_ptr, b_val );

   // Will put 'a' first in the cache to evict 'b'
   cache.get( a_key );

   std::string fill_key = "f";
   std::string fill_val( cache_size - a_val.size() - b_val.size() + 1, 'f' );
   BOOST_CHECK( cache.put( fill_key, fill_val ) );
   BOOST_CHECK( !cache.get( b_key ) );

   val_ptr = cache.get( a_key );
   BOOST_REQUIRE( val_ptr );
   BOOST_CHECK_EQUAL( *val_ptr, a_val );

   BOOST_CHECK( cache.put( fill_key, fill_val ) );
   BOOST_CHECK( !cache.get( b_key ) );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( map_backend_test )
{ try {
   koinos::state_db::backends::map::map_backend backend;

   auto itr = backend.begin();
   BOOST_CHECK( itr == backend.end() );

   backend.put( "foo", "bar" );
   itr = backend.begin();
   BOOST_CHECK( itr != backend.end() );
   BOOST_CHECK( *itr == "bar" );

   backend.put( "alice", "bob" );

   itr = backend.begin();
   BOOST_CHECK( itr != backend.end() );
   BOOST_CHECK( *itr == "bob" );

   ++itr;
   BOOST_CHECK( *itr == "bar" );

   ++itr;
   BOOST_CHECK( itr == backend.end() );

   --itr;
   BOOST_CHECK( itr != backend.end() );
   BOOST_CHECK( *itr == "bar" );

   itr = backend.lower_bound( "charlie" );
   BOOST_CHECK( itr != backend.end() );
   BOOST_CHECK( *itr == "bar" );

   itr = backend.lower_bound( "foo" );
   BOOST_CHECK( itr != backend.end() );
   BOOST_CHECK( *itr == "bar" );

   backend.put( "foo", "blob" );
   itr = backend.find( "foo" );
   BOOST_CHECK( itr != backend.end() );
   BOOST_CHECK( *itr == "blob" );

   --itr;
   BOOST_CHECK( itr != backend.end() );
   BOOST_CHECK( *itr == "bob" );

   backend.erase( "foo" );

   itr = backend.begin();
   BOOST_CHECK( itr != backend.end() );
   BOOST_CHECK( *itr == "bob" );

   itr = backend.find( "foo" );
   BOOST_CHECK( itr == backend.end() );

   backend.erase( "foo" );

   backend.erase( "alice" );
   itr = backend.end();
   BOOST_CHECK( itr == backend.end() );

   backend.put( "foo", "bar" );
   BOOST_REQUIRE( backend.get( "foo" ) );
   BOOST_CHECK_EQUAL( *backend.get( "foo" ), "bar" );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_SUITE_END()
