
#include <boost/test/unit_test.hpp>

#include <koinos/capsule/capsule.hpp>
#include <koinos/tests/capsule/dummy_hash.hpp>
#include <koinos/tests/capsule/dummy_operations.hpp>
#include <koinos/tests/capsule/dummy_tree.hpp>

#include <memory>
#include <string>
#include <tuple>
#include <variant>

struct capsule_fixture
{
   capsule_fixture()
   {
   }

   ~capsule_fixture()
   {
   }
};

BOOST_FIXTURE_TEST_SUITE( capsule_tests, capsule_fixture )

using namespace koinos::capsule;

BOOST_AUTO_TEST_CASE( dummy_hash_test )
{ try {
   BOOST_TEST_MESSAGE( "Checking dummy hash functionality" );
   auto h25 = create_dummy_hash(25);
   auto h29 = create_dummy_hash(29);
   auto h37 = create_dummy_hash(37);
   auto ha = reduce_dummy_hash(h25, h29);
   auto hb = reduce_dummy_hash(ha, h37);
   BOOST_CHECK_EQUAL( dummy_hash_to_string(hb), "((h25, h29), h37)" );
} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( tree_walker_test_table_cases )
{ try {
   BOOST_TEST_MESSAGE( "Tree walker functionality" );

   //                                15
   //                         /               \
   //                 7                              23
   //             /       \                       /      \
   //         3              11              19              27
   //       /   \          /    \          /    \          /    \
   //     1       5       9      13      17      21      25      29
   //    / \     / \     / \     / \     / \     / \     / \     / \
   //   0   2   4   6   8  10  12   14 16   18 20   22 24   26 28   30

   std::vector< std::tuple< int64_t, std::vector< int64_t >, std::string > > cases = {
      {32, {}, "h-1"},
      {32, {9, 11}, "(h7, (((h16, h18), (h20, h22)), h27))"},
      {32, {9, 12}, "(h7, (((h16, h18), h21), ((h24, h26), h29)))"}
      };

   for( const auto& c : cases )
   {
      int64_t size;
      std::vector< int64_t > objects;
      std::string expected_output;
      std::tie( size, objects, expected_output ) = c;
      dummy_operations ops(32);
      tree_walker< dummy_hash_ptr, dummy_operations > w(&ops);
      for( int64_t o : objects )
         w.add_object( o, create_dummy_hash(2*o) );
      dummy_hash_ptr h = w.close();
      BOOST_CHECK_EQUAL( dummy_hash_to_string(h), expected_output );
   }
} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( tree_walker_test_exhaust )
{ try {
   BOOST_TEST_MESSAGE( "Exhaustive test of tree walker for all 2^16 possible combinations of leaf nodes" );

   // Increase this to test a larger one
   const int64_t TREE_LEVEL = 4;
   const int64_t NUM_BITS = int64_t(1) << TREE_LEVEL;

   for( int64_t has_node=1; has_node<(int64_t(1) << NUM_BITS); has_node++ )
   {
      // BOOST_TEST_MESSAGE( "has_node = " + std::to_string(has_node) );
      std::set< int64_t > active_leaf_ids;
      for( int64_t i=0; i<NUM_BITS; i++ )
      {
         if( (has_node & (1 << i)) != 0 )
            active_leaf_ids.insert(i*2);
      }

      dummy_tree_ptr tree = create_tree(TREE_LEVEL);
      annotate_leaves(tree, tree->root_id);

      // std::set< int64_t > none;
      // print_dummy_tree(tree, none);

      std::set< int64_t > pruned = prune(tree, active_leaf_ids, false);

      // print_dummy_tree(tree, pruned);

      std::string expected_output = pruned_tree_to_string(tree, tree->root_id, pruned);

      dummy_operations ops(2*NUM_BITS);
      tree_walker< dummy_hash_ptr, dummy_operations > w(&ops);
      for( int64_t i=0; i<NUM_BITS; i++ )
      {
         if( (has_node & (1 << i)) != 0 )
            w.add_object( i, create_dummy_hash(2*i) );
      }
      dummy_hash_ptr h = w.close();
      BOOST_REQUIRE_EQUAL( dummy_hash_to_string(h), expected_output );
   }

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_SUITE_END()
