#include <boost/test/unit_test.hpp>

#include <koinos/fork/fork_database.hpp>
#include <algorithm>
#include <functional>
#include <string>

using namespace koinos::fork;

struct test_block
{
   std::size_t id;
   std::size_t previous;
   uint64_t block_num;
};

using fork_database_type = fork_database< test_block >;
using block_state_type   = block_state< test_block >;

struct fork_fixture
{
   fork_database_type::block_state_ptr make_block(
      fork_database_type::block_num_type block_num,
      fork_database_type::block_id_type previous_id = fork_database_type::block_id_type(),
      std::string seed = "" )
   {
      test_block b;
      b.block_num = block_num;
      b.previous  = previous_id;
      b.id        = std::hash< std::string >{}( std::to_string( b.block_num ) + seed );
      return std::make_shared< block_state_type >( b );
   }
};

BOOST_FIXTURE_TEST_SUITE( fork_tests, fork_fixture )

BOOST_AUTO_TEST_CASE( fork_tests )
{
   BOOST_TEST_MESSAGE( "basic fork database setup" );
   fork_database_type fork_db;
   auto prev = make_block( 1 );
   fork_db.reset( prev );

   for ( fork_database_type::block_num_type i = 2; i <= 2000; ++i )
   {
      auto b = make_block( i, prev->id() );
      fork_db.add( b );
      prev = b;
   }

   BOOST_REQUIRE( fork_db.root()->block_num() == 1 );
   BOOST_REQUIRE( fork_db.head()->block_num() == 2000 );

   BOOST_TEST_MESSAGE( "check advance root" );
   auto new_root = fork_db.search_on_branch( fork_db.head()->id(), 1000 );
   fork_db.advance_root( new_root->id() );

   BOOST_REQUIRE( fork_db.root()->block_num() == 1000 );
   BOOST_REQUIRE( fork_db.head()->block_num() == 2000 );

   for ( fork_database_type::block_num_type i = 1; i <= 999; i++ )
   {
      BOOST_REQUIRE( fork_db.fetch_block_by_number( i ).empty() );
   }

   std::vector< fork_database_type::block_id_type > b1_ids;
   std::vector< fork_database_type::block_id_type > b2_ids;
   auto b0_prev = prev;
   auto b1_prev = prev;
   auto b2_prev = prev;
   for ( fork_database_type::block_num_type i = 2001; i <= 2050; i++ )
   {
      auto b0 = make_block( i, b0_prev->id() );
      auto b1 = make_block( i, b1_prev->id(), "branch_1" );
      auto b2 = make_block( i, b2_prev->id(), "branch_2" );
      b1_ids.push_back( b1->id() );
      b2_ids.push_back( b2->id() );
      fork_db.add( b0 );
      fork_db.add( b1 );
      fork_db.add( b2 );
      b0_prev = b0;
      b1_prev = b1;
      b2_prev = b2;
   }
   prev = b0_prev;

   BOOST_TEST_MESSAGE( "check fetch branch from" );
   auto branch_pair = fork_db.fetch_branch_from( b1_ids.back(), b2_ids.back() );

   std::reverse( b1_ids.begin(), b1_ids.end() );
   for ( size_t i = 0; i < branch_pair.first.size(); i++ )
   {
      BOOST_REQUIRE( branch_pair.first[i]->id() == b1_ids[i] );
   }

   std::reverse( b2_ids.begin(), b2_ids.end() );
   for ( size_t i = 0; i < branch_pair.second.size(); i++ )
   {
      BOOST_REQUIRE( branch_pair.second[i]->id() == b2_ids[i] );
   }

   BOOST_REQUIRE( branch_pair.first.back()->previous_id() == branch_pair.second.back()->previous_id() );

   for ( fork_database_type::block_num_type i = 2051; i <= 3000; i++ )
   {
      auto b = make_block( i, prev->id() );
      fork_db.add( b );
      prev = b;
   }

   BOOST_REQUIRE( fork_db.root()->block_num() == 1000 );
   BOOST_REQUIRE( fork_db.head()->block_num() == 3000 );
   BOOST_REQUIRE( fork_db.head()->id() == prev->id() );

   new_root = fork_db.search_on_branch( fork_db.head()->id(), 2001 );

   BOOST_TEST_MESSAGE( "check fetch block by number" );
   auto blocks = fork_db.fetch_block_by_number( 2001 );

   BOOST_REQUIRE( blocks.size() == 3 );
   for ( const auto& b : blocks )
   {
      
         bool ok = std::find( b1_ids.begin(), b1_ids.end(), b->id() ) != b1_ids.end() ||
         std::find( b2_ids.begin(), b2_ids.end(), b->id() ) != b2_ids.end() ||
	   b->id() == new_root->id();

	 BOOST_REQUIRE( ok );
   }

   fork_db.advance_root( new_root->id() );

   BOOST_TEST_MESSAGE( "check removal of ids that link to a removed block" );
   std::vector< fork_database_type::block_id_type > removed_ids;
   removed_ids.reserve( b1_ids.size() + b2_ids.size() );
   removed_ids.insert( removed_ids.end(), b1_ids.begin(), b1_ids.end() );
   removed_ids.insert( removed_ids.end(), b2_ids.begin(), b2_ids.end() );

   for ( const auto& id : removed_ids )
   {
      BOOST_REQUIRE( fork_db.fetch_block( id ) == nullptr );
   }

   BOOST_REQUIRE( fork_db.root()->block_num() == 2001 );
   BOOST_REQUIRE( fork_db.head()->block_num() == 3000 );

   BOOST_TEST_MESSAGE( "check duplicate block exception" );
   BOOST_CHECK_THROW( fork_db.add( fork_db.head(), false ), duplicate_block_exception );

   BOOST_TEST_MESSAGE( "check unlinkable block exception" );
   BOOST_CHECK_THROW( fork_db.add( make_block( 3001 ) ), unlinkable_block_exception );
}

BOOST_AUTO_TEST_SUITE_END()
