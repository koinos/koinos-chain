#define BOOST_TEST_MODULE mira test

#include "test_objects.hpp"
#include "test_templates.hpp"

#include <boost/test/unit_test.hpp>
#include <mira/database_configuration.hpp>
#include <iostream>

using namespace mira;

using mira::multi_index::multi_index_container;
using mira::multi_index::indexed_by;
using mira::multi_index::ordered_unique;
using mira::multi_index::tag;
using mira::multi_index::member;
using mira::multi_index::composite_key;
using mira::multi_index::composite_key_compare;
using mira::multi_index::const_mem_fun;

struct mira_fixture {
   boost::filesystem::path tmp_path;
   std::any cfg;

   mira_fixture()
   {
      tmp_path = boost::filesystem::current_path() / boost::filesystem::unique_path();
      boost::filesystem::create_directory( tmp_path );
      cfg = mira::utilities::default_database_configuration();
   }

   ~mira_fixture()
   {
      boost::filesystem::remove_all( tmp_path );
   }
};

BOOST_FIXTURE_TEST_SUITE( mira_tests, mira_fixture )

BOOST_AUTO_TEST_CASE( sanity_tests )
{
   auto index = book_index();
   index.open( tmp_path, cfg, index_type::mira );

   BOOST_TEST_MESSAGE( "Creating book" );
   {
      book b;
      b.id = 0;
      b.a = 3;
      b.b = 4;

      auto new_book_itr = index.emplace( std::move( b ) );

      BOOST_REQUIRE( new_book_itr.second );
      BOOST_REQUIRE_EQUAL( new_book_itr.first->id, 0 );
      BOOST_REQUIRE_EQUAL( new_book_itr.first->a, 3 );
      BOOST_REQUIRE_EQUAL( new_book_itr.first->b, 4 );
   }



   {
      book b;
      b.id = 1;
      b.a = 3;
      b.b = 5;

      BOOST_REQUIRE( index.emplace( std::move( b ) ).second == false );
   }

   {
      book b;
      b.id = 1;
      b.a = 4;
      b.b = 5;

      index.emplace( std::move( b ) );
   }

   {
      book b;
      b.id = 2;
      b.a = 2;
      b.b = 1;

      index.emplace( std::move( b ) );
   }

   const auto& book_idx = index.get< by_id >();
   auto itr = book_idx.begin();

   BOOST_REQUIRE( itr != book_idx.end() );

   {
      const auto& tmp_book = *itr;

      BOOST_REQUIRE( tmp_book.id == 0 );
      BOOST_REQUIRE( tmp_book.a == 3 );
      BOOST_REQUIRE( tmp_book.b == 4 );
   }

   ++itr;

   {
      const auto& tmp_book = *itr;

      BOOST_REQUIRE( tmp_book.id == 1 );
      BOOST_REQUIRE( tmp_book.a == 4 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   ++itr;

   {
      const auto& tmp_book = *itr;

      BOOST_REQUIRE( tmp_book.id == 2 );
      BOOST_REQUIRE( tmp_book.a == 2 );
      BOOST_REQUIRE( tmp_book.b == 1 );
   }

   ++itr;

   BOOST_REQUIRE( itr == book_idx.end() );

   itr = book_idx.end();

   BOOST_REQUIRE( itr == book_idx.end() );

   --itr;

   {
      const auto& tmp_book = *itr;

      BOOST_REQUIRE( tmp_book.id == 2 );
      BOOST_REQUIRE( tmp_book.a == 2 );
      BOOST_REQUIRE( tmp_book.b == 1 );
   }

   --itr;

   {
      const auto& tmp_book = *itr;

      BOOST_REQUIRE( tmp_book.id == 1 );
      BOOST_REQUIRE( tmp_book.a == 4 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   --itr;

   {
      const auto& tmp_book = *itr;

      BOOST_REQUIRE( tmp_book.id == 0 );
      BOOST_REQUIRE( tmp_book.a == 3 );
      BOOST_REQUIRE( tmp_book.b == 4 );
   }

   const auto& book_by_a_idx = index.get< by_a >();
   auto a_itr = book_by_a_idx.begin();

   {
      const auto& tmp_book = *a_itr;

      BOOST_REQUIRE( tmp_book.id == 2 );
      BOOST_REQUIRE( tmp_book.a == 2 );
      BOOST_REQUIRE( tmp_book.b == 1 );
   }

   ++a_itr;

   {
      const auto& tmp_book = *a_itr;

      BOOST_REQUIRE( tmp_book.id == 0 );
      BOOST_REQUIRE( tmp_book.a == 3 );
      BOOST_REQUIRE( tmp_book.b == 4 );
   }

   ++a_itr;

   {
      const auto& tmp_book = *a_itr;

      BOOST_REQUIRE( tmp_book.id == 1 );
      BOOST_REQUIRE( tmp_book.a == 4 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   {
      const auto& tmp_book = *(book_by_a_idx.lower_bound( 3 ));

      BOOST_REQUIRE( tmp_book.id == 0 );
      BOOST_REQUIRE( tmp_book.a == 3 );
      BOOST_REQUIRE( tmp_book.b == 4 );
   }

   BOOST_REQUIRE( book_by_a_idx.lower_bound( 5 ) == book_by_a_idx.end() );

   {
      const auto& tmp_book = *(book_by_a_idx.upper_bound( 3 ));

      BOOST_REQUIRE( tmp_book.id == 1 );
      BOOST_REQUIRE( tmp_book.a == 4 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   BOOST_REQUIRE( book_by_a_idx.upper_bound( 5 ) == book_by_a_idx.end() );

   {
      const auto& tmp_book = *(index.get< by_id >().find( 1 ));

      BOOST_REQUIRE( tmp_book.id == 1 );
      BOOST_REQUIRE( tmp_book.a == 4 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   {
      const auto book_ptr = index.get< by_a >().find( 4 );

      BOOST_REQUIRE( book_ptr->id == 1 );
      BOOST_REQUIRE( book_ptr->a == 4 );
      BOOST_REQUIRE( book_ptr->b == 5 );
   }

   bool is_found = index.get< by_a >().find( 10 ) != index.get< by_a >().end();

   BOOST_REQUIRE( !is_found );

   const auto& book_by_b_idx = index.get< by_b >();
   auto b_itr = book_by_b_idx.begin();

   BOOST_REQUIRE( b_itr != book_by_b_idx.end() );

   {
      const auto& tmp_book = *b_itr;

      BOOST_REQUIRE( tmp_book.id == 1 );
      BOOST_REQUIRE( tmp_book.a == 4 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   ++b_itr;

   {
      const auto& tmp_book = *b_itr;

      BOOST_REQUIRE( tmp_book.id == 0 );
      BOOST_REQUIRE( tmp_book.a == 3 );
      BOOST_REQUIRE( tmp_book.b == 4 );
   }

   ++b_itr;

   {
      const auto& tmp_book = *b_itr;

      BOOST_REQUIRE( tmp_book.id == 2 );
      BOOST_REQUIRE( tmp_book.a == 2 );
      BOOST_REQUIRE( tmp_book.b == 1 );
   }

   ++b_itr;

   BOOST_REQUIRE( b_itr == book_by_b_idx.end() );

   const auto book_by_b = *(index.get< by_b >().find( boost::make_tuple( 5, 4 ) ));

   BOOST_REQUIRE( book_by_b.id == 1 );
   BOOST_REQUIRE( book_by_b.a == 4 );
   BOOST_REQUIRE( book_by_b.b == 5 );

   b_itr = book_by_b_idx.lower_bound( 10 );

   {
      const auto& tmp_book = *b_itr;

      BOOST_REQUIRE( tmp_book.id == 1 );
      BOOST_REQUIRE( tmp_book.a == 4 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   const auto& book_by_sum_idx = index.get< by_sum >();
   auto by_sum_itr = book_by_sum_idx.begin();

   {
      const auto& tmp_book = *by_sum_itr;

      BOOST_REQUIRE( tmp_book.id == 2 );
      BOOST_REQUIRE( tmp_book.a == 2 );
      BOOST_REQUIRE( tmp_book.b == 1 );
   }

   ++by_sum_itr;

   {
      const auto& tmp_book = *by_sum_itr;

      BOOST_REQUIRE( tmp_book.id == 0 );
      BOOST_REQUIRE( tmp_book.a == 3 );
      BOOST_REQUIRE( tmp_book.b == 4 );
   }

   ++by_sum_itr;

   {
      const auto& tmp_book = *by_sum_itr;

      BOOST_REQUIRE( tmp_book.id == 1 );
      BOOST_REQUIRE( tmp_book.a == 4 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   ++by_sum_itr;

   BOOST_REQUIRE( by_sum_itr == book_by_sum_idx.end() );

   const auto& book_by_id = *(index.get< by_id >().find( 0 ));
   const auto& book_by_a = *(index.get< by_a >().find( 3 ));

   BOOST_REQUIRE( &book_by_id == &book_by_a );

   index.modify( index.get< by_id >().find( 0 ), []( book& b )
   {
      b.a = 10;
      b.b = 5;
   });

   {
      const auto& tmp_book = *(index.get< by_id >().find( 0 ));

      BOOST_REQUIRE( tmp_book.id == 0 );
      BOOST_REQUIRE( tmp_book.a == 10 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   // Failure due to collision on 'a'
   BOOST_REQUIRE(
      index.modify( index.get< by_id >().find( 0 ), []( book& b )
      {
         b.a = 4;
         b.b = 10;
      }) == false );

   {
      const auto& tmp_book = *(index.get< by_id >().find( 0 ));

      BOOST_REQUIRE( tmp_book.id == 0 );
      BOOST_REQUIRE( tmp_book.a == 10 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   // Failure due to collision on 'sum'
   BOOST_REQUIRE( index.modify( index.get< by_id >().find( 0 ), []( book& b )
      {
         b.a = 6;
         b.b = 3;
      }) == false );

   {
      const auto& tmp_book = *(index.get< by_id >().find( 0 ));

      BOOST_REQUIRE( tmp_book.id == 0 );
      BOOST_REQUIRE( tmp_book.a == 10 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   a_itr = book_by_a_idx.begin();

   BOOST_REQUIRE( a_itr != book_by_a_idx.end() );

   {
      const auto& tmp_book = *a_itr;

      BOOST_REQUIRE( tmp_book.id == 2 );
      BOOST_REQUIRE( tmp_book.a == 2 );
      BOOST_REQUIRE( tmp_book.b == 1 );
   }

   ++a_itr;

   {
      const auto& tmp_book = *a_itr;

      BOOST_REQUIRE( tmp_book.id == 1 );
      BOOST_REQUIRE( tmp_book.a == 4 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   ++a_itr;

   {
      const auto& tmp_book = *a_itr;

      BOOST_REQUIRE( tmp_book.id == 0 );
      BOOST_REQUIRE( tmp_book.a == 10 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   ++a_itr;

   BOOST_REQUIRE( a_itr == book_by_a_idx.end() );

   b_itr = book_by_b_idx.begin();

   BOOST_REQUIRE( b_itr != book_by_b_idx.end() );

   {
      const auto& tmp_book = *b_itr;

      BOOST_REQUIRE( tmp_book.id == 1 );
      BOOST_REQUIRE( tmp_book.a == 4 );
      BOOST_REQUIRE( tmp_book.b == 5 );

   }

   ++b_itr;

   {
      const auto& tmp_book = *b_itr;

      BOOST_REQUIRE( tmp_book.id == 0 );
      BOOST_REQUIRE( tmp_book.a == 10 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   ++b_itr;

   {
      const auto& tmp_book = *b_itr;

      BOOST_REQUIRE( tmp_book.id == 2 );
      BOOST_REQUIRE( tmp_book.a == 2 );
      BOOST_REQUIRE( tmp_book.b == 1 );
   }

   ++b_itr;

   BOOST_REQUIRE( b_itr == book_by_b_idx.end() );

   b_itr = book_by_b_idx.lower_bound( boost::make_tuple( 5, 5 ) );

   {
      const auto& tmp_book = *b_itr;

      BOOST_REQUIRE( tmp_book.id == 0 );
      BOOST_REQUIRE( tmp_book.a == 10 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   by_sum_itr = book_by_sum_idx.begin();

   {
      const auto& tmp_book = *by_sum_itr;

      BOOST_REQUIRE( tmp_book.id == 2 );
      BOOST_REQUIRE( tmp_book.a == 2 );
      BOOST_REQUIRE( tmp_book.b == 1 );
   }

   ++by_sum_itr;

   {
      const auto& tmp_book = *by_sum_itr;

      BOOST_REQUIRE( tmp_book.id == 1 );
      BOOST_REQUIRE( tmp_book.a == 4 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   ++by_sum_itr;

   {
      const auto& tmp_book = *by_sum_itr;

      BOOST_REQUIRE( tmp_book.id == 0 );
      BOOST_REQUIRE( tmp_book.a == 10 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   ++by_sum_itr;

   BOOST_REQUIRE( by_sum_itr == book_by_sum_idx.end() );

   index.erase( index.get< by_id >().find( 0 ) );

   is_found = index.get< by_id >().find( 0 ) != index.get< by_id >().end();

   BOOST_REQUIRE( !is_found );

   itr = book_idx.begin();

   BOOST_REQUIRE( itr != book_idx.end() );

   {
      const auto& tmp_book = *itr;

      BOOST_REQUIRE( tmp_book.id == 1 );
      BOOST_REQUIRE( tmp_book.a == 4 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   ++itr;

   {
      const auto& tmp_book = *itr;

      BOOST_REQUIRE( tmp_book.id == 2 );
      BOOST_REQUIRE( tmp_book.a == 2 );
      BOOST_REQUIRE( tmp_book.b == 1 );
   }

   ++itr;

   BOOST_REQUIRE( itr == book_idx.end() );

   a_itr = book_by_a_idx.begin();

   BOOST_REQUIRE( a_itr != book_by_a_idx.end() );

   {
      const auto& tmp_book = *a_itr;

      BOOST_REQUIRE( tmp_book.id == 2 );
      BOOST_REQUIRE( tmp_book.a == 2 );
      BOOST_REQUIRE( tmp_book.b == 1 );
   }

   ++a_itr;

   {
      const auto& tmp_book = *a_itr;

      BOOST_REQUIRE( tmp_book.id == 1 );
      BOOST_REQUIRE( tmp_book.a == 4 );
      BOOST_REQUIRE( tmp_book.b == 5 );
   }

   ++a_itr;

   BOOST_REQUIRE( a_itr == book_by_a_idx.end() );
}

BOOST_AUTO_TEST_CASE( single_index_test )
{
   auto index = single_index_index();
   index.open( tmp_path, cfg, index_type::mira );

   single_index_object o;
   o.id = 0;
   index.emplace( std::move( o ) );

   const auto& sio = *(index.find( 0 ));

   BOOST_REQUIRE( sio.id == 0 );
}

BOOST_AUTO_TEST_CASE( variable_length_key_test )
{
   auto index = account_index();
   index.open( tmp_path, cfg, index_type::mira );

   const auto& acc_by_name_idx = index.get< by_name >();
   auto itr = acc_by_name_idx.begin();

   BOOST_REQUIRE( itr == acc_by_name_idx.end() );

   {
      account_object a;
      a.id = 0;
      a.name = "alice";

      index.emplace( std::move( a ) );
   }

   {
      account_object a;
      a.id = 1;
      a.name = "bob";

      index.emplace( std::move( a ) );
   }

   {
      account_object a;
      a.id = 2;
      a.name = "charlie";

      index.emplace( std::move( a ) );
   }

   itr = acc_by_name_idx.begin();

   BOOST_REQUIRE( itr->name == "alice" );

   ++itr;

   BOOST_REQUIRE( itr->name == "bob" );

   ++itr;

   BOOST_REQUIRE( itr->name == "charlie" );

   ++itr;

   BOOST_REQUIRE( itr == acc_by_name_idx.end() );

   itr = acc_by_name_idx.lower_bound( "archibald" );

   BOOST_REQUIRE( itr->name == "bob" );
}

BOOST_AUTO_TEST_CASE( sanity_modify_test )
{
   auto index = book_index();
   index.open( tmp_path, cfg, index_type::mira );

   book b1_obj;
   b1_obj.id = 0;
   b1_obj.a = 1;
   b1_obj.b = 2;
   auto b1 = index.emplace( std::move( b1_obj ) );

   book b2_obj;
   b2_obj.id = 1;
   b2_obj.a = 2;
   b2_obj.b = 3;
   auto b2 = index.emplace( std::move( b2_obj ) );

   book b3_obj;
   b3_obj.id = 2;
   b3_obj.a = 4;
   b3_obj.b = 5;
   auto b3 = index.emplace( std::move( b3_obj ) );

   BOOST_REQUIRE( b1.first->a == 1 );
   BOOST_REQUIRE( b1.first->b == 2 );
   BOOST_REQUIRE( b1.first->sum() == 3 );

   BOOST_REQUIRE( b2.first->a == 2 );
   BOOST_REQUIRE( b2.first->b == 3 );
   BOOST_REQUIRE( b2.first->sum() == 5 );

   BOOST_REQUIRE( b3.first->a == 4 );
   BOOST_REQUIRE( b3.first->b == 5 );
   BOOST_REQUIRE( b3.first->sum() == 9 );

   index.modify( index.iterator_to( *(b2.first) ), [] ( book& b )
   {
      b.a = 10;
      b.b = 20;
   } );

   BOOST_REQUIRE( b2.first->a == 10 );
   BOOST_REQUIRE( b2.first->b == 20 );
   BOOST_REQUIRE( b2.first->sum() == 30 );

   auto idx_by_a = index.get< by_a >();
   auto bb = idx_by_a.end();
   --bb;

   BOOST_REQUIRE( bb->a == 10 );
   BOOST_REQUIRE( bb->b == 20 );
   BOOST_REQUIRE( bb->sum() == 30 );
   BOOST_REQUIRE( &*bb == &*(b2.first) );

   auto idx_by_sum = index.get< by_sum >();
   auto bb2 = idx_by_sum.end();
   --bb2;

   BOOST_REQUIRE( bb2->a == 10 );
   BOOST_REQUIRE( bb2->b == 20 );
   BOOST_REQUIRE( bb2->sum() == 30 );
   BOOST_REQUIRE( &*bb2 == &*(b2.first) );
}

BOOST_AUTO_TEST_CASE( range_test )
{
   auto index = test_object3_index();
   index.open( tmp_path, cfg, index_type::mira );

   for ( uint32_t i = 0; i < 10; i++ )
   {
      for ( uint32_t j = 0; j < 10; j++ )
      {
         test_object3 o;
         o.id = i * 10 + j;
         o.val = i;
         o.val2 = j;
         o.val3 = i + j;

         index.emplace( std::move( o ) );
      }
   }

   const auto& idx = index.get< composite_ordered_idx3a >();

   auto er = idx.equal_range( 5 );

   BOOST_REQUIRE( er.first->val == 5 );
   BOOST_REQUIRE( er.first->val2 == 0 );

   BOOST_REQUIRE( er.second->val == 6 );
   BOOST_REQUIRE( er.second->val2 == 0 );

   auto er2 = idx.equal_range( 9 );

   BOOST_REQUIRE( er2.first->val == 9 );
   BOOST_REQUIRE( er2.first->val2 == 0 );

   BOOST_REQUIRE( er2.second == idx.end() );
}

BOOST_AUTO_TEST_CASE( bounds_test )
{
   auto index = test_object3_index();
   index.open( tmp_path, cfg, index_type::mira );

   for ( uint32_t i = 0; i < 10; i++ )
   {
      for ( uint32_t j = 0; j < 10; j++ )
      {
         test_object3 o;
         o.id = i * 10 + j;
         o.val = i;
         o.val2 = j;
         o.val3 = i + j;

         index.emplace( std::move( o ) );
      }
   }

   const auto& idx = index.get< composite_ordered_idx3a >();

   auto upper_bound_not_found = idx.upper_bound( 10 );
   BOOST_REQUIRE( upper_bound_not_found == idx.end() );

   auto lower_bound_not_found = idx.lower_bound( 10 );
   BOOST_REQUIRE( lower_bound_not_found == idx.end() );

   auto composite_lower_bound = idx.lower_bound( boost::make_tuple( 3, 1 ) );

   BOOST_REQUIRE( composite_lower_bound->val == 3 );
   BOOST_REQUIRE( composite_lower_bound->val2 == 1 );

   auto composite_upper_bound = idx.upper_bound( boost::make_tuple( 3, 5 ) );

   BOOST_REQUIRE( composite_upper_bound->val == 3 );
   BOOST_REQUIRE( composite_upper_bound->val2 == 6 );

   auto lower_iter = idx.lower_bound( 5 );

   BOOST_REQUIRE( lower_iter->val == 5 );
   BOOST_REQUIRE( lower_iter->val2 == 0 );

   auto upper_iter = idx.upper_bound( 5 );

   BOOST_REQUIRE( upper_iter->val == 6 );
   BOOST_REQUIRE( upper_iter->val2 == 0 );
}

BOOST_AUTO_TEST_CASE( basic_tests )
{
   auto index = test_object_index();
   index.open( tmp_path, cfg, index_type::mira );

   auto index2 = test_object2_index();
   index2.open( tmp_path, cfg, index_type::mira );

   auto index3 = test_object3_index();
   index3.open( tmp_path, cfg, index_type::mira );

   auto c1 = []( test_object& obj ) { obj.name = "_name"; };
   auto c1b = []( test_object2& obj ) {};
   auto c1c = []( test_object3& obj ) { obj.val2 = 5; obj.val3 = 5; };

   basic_test< test_object_index, test_object, ordered_idx >( { 0, 1, 2, 3, 4, 5 }, c1, index );
   basic_test< test_object2_index, test_object2, ordered_idx2 >( { 0, 1, 2 }, c1b, index2 );
   basic_test< test_object3_index, test_object3, ordered_idx3 >( { 0, 1, 2, 3, 4 }, c1c, index3 );
}

BOOST_AUTO_TEST_CASE( insert_remove_tests )
{
   auto index = test_object_index();
   index.open( tmp_path, cfg, index_type::mira );

   auto index2 = test_object2_index();
   index2.open( tmp_path, cfg, index_type::mira );

   auto index3 = test_object3_index();
   index3.open( tmp_path, cfg, index_type::mira );

   auto c1 = []( test_object& obj ) { obj.name = "_name"; };
   auto c1b = []( test_object2& obj ) {};
   auto c1c = []( test_object3& obj ) { obj.val2 = 7; obj.val3 = obj.val2 + 1; };

   insert_remove_test< test_object_index, test_object, ordered_idx >( { 0, 1, 2, 3, 4, 5, 6, 7 }, c1, index );
   insert_remove_test< test_object2_index, test_object2, ordered_idx2 >( { 0, 1, 2, 3, 4, 5, 6, 7 }, c1b, index2 );
   insert_remove_test< test_object3_index, test_object3, ordered_idx3 >( { 0, 1, 2, 3 }, c1c, index3 );
}

BOOST_AUTO_TEST_CASE( insert_remove_collision_tests )
{
   auto index = test_object_index();
   index.open( tmp_path, cfg, index_type::mira );

   auto index2 = test_object2_index();
   index2.open( tmp_path, cfg, index_type::mira );

   auto index3 = test_object3_index();
   index3.open( tmp_path, cfg, index_type::mira );

   auto c1 = []( test_object& obj ) { obj.id = 0; obj.name = "_name7"; obj.val = 7; };
   auto c2 = []( test_object& obj ) { obj.id = 0; obj.name = "_name8"; obj.val = 8; };
   auto c3 = []( test_object& obj ) { obj.id = 0; obj.name = "the_same_name"; obj.val = 7; };
   auto c4 = []( test_object& obj ) { obj.id = 1; obj.name = "the_same_name"; obj.val = 7; };

   auto c1b = []( test_object2& obj ) { obj.id = 0; obj.val = 7; };
   auto c2b = []( test_object2& obj ) { obj.id = 0; obj.val = 8; };
   auto c3b = []( test_object2& obj ) { obj.id = 6; obj.val = 7; };
   auto c4b = []( test_object2& obj ) { obj.id = 6; obj.val = 7; };

   auto c1c = []( test_object3& obj ) { obj.id = 0; obj.val = 20; obj.val2 = 20; };
   auto c2c = []( test_object3& obj ) { obj.id = 1; obj.val = 20; obj.val2 = 20; };
   auto c3c = []( test_object3& obj ) { obj.id = 2; obj.val = 30; obj.val3 = 30; };
   auto c4c = []( test_object3& obj ) { obj.id = 3; obj.val = 30; obj.val3 = 30; };

   insert_remove_collision_test< test_object_index, test_object, ordered_idx >( {}, c1, c2, c3, c4, index );
   insert_remove_collision_test< test_object2_index, test_object2, ordered_idx2 >( {}, c1b, c2b, c3b, c4b, index2 );
   insert_remove_collision_test< test_object3_index, test_object3, ordered_idx3 >( {}, c1c, c2c, c3c, c4c, index3 );
}

BOOST_AUTO_TEST_CASE( modify_tests )
{
   auto index = test_object_index();
   index.open( tmp_path, cfg, index_type::mira );

   auto index2 = test_object2_index();
   index2.open( tmp_path, cfg, index_type::mira );

   auto c1 = []( test_object& obj ) { obj.name = "_name"; };
   auto c2 = []( test_object& obj ){ obj.name = "new_name"; };
   auto c3 = []( const test_object& obj ){ BOOST_REQUIRE( obj.name == "new_name" ); };
   auto c4 = []( const test_object& obj ){ BOOST_REQUIRE( obj.val == obj.id + 100 ); };
   auto c5 = []( bool result ){ BOOST_REQUIRE( result == false ); };

   auto c1b = []( test_object2& obj ) { obj.val = 889; };
   auto c2b = []( test_object2& obj ){ obj.val = 2889; };
   auto c3b = []( const test_object2& obj ){ BOOST_REQUIRE( obj.val == 2889 ); };
   auto c4b = []( const test_object2& obj ){ /*empty*/ };
   auto c5b = []( bool result ){ BOOST_REQUIRE( result == true ); };

   modify_test< test_object_index, test_object, ordered_idx >( { 0, 1, 2, 3 }, c1, c2, c3, c4, c5, index );
   modify_test< test_object2_index, test_object2, ordered_idx2 >( { 0, 1, 2, 3, 4, 5 }, c1b, c2b, c3b, c4b, c5b, index2 );
}

BOOST_AUTO_TEST_CASE( misc_tests )
{
   auto index = test_object_index();
   index.open( tmp_path, cfg, index_type::mira );

   misc_test< test_object_index, test_object, ordered_idx, composited_ordered_idx >( { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }, index );
}

BOOST_AUTO_TEST_CASE( misc_tests3 )
{
   auto index = test_object3_index();
   index.open( tmp_path, cfg, index_type::mira );

   misc_test3< test_object3_index, test_object3, ordered_idx3, composite_ordered_idx3a, composite_ordered_idx3b >( { 0, 1, 2 }, index );
}

BOOST_AUTO_TEST_SUITE_END()
