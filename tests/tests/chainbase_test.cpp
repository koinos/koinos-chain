#include "../test_fixtures/chainbase_fixture.hpp"

#include <boost/any.hpp>

#include <mira/database_configuration.hpp>

#include <iostream>

BOOST_FIXTURE_TEST_SUITE( chainbase_tests, chainbase_fixture )

using namespace chainbase;

BOOST_AUTO_TEST_CASE( basic_test )
{
   boost::filesystem::path temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
   try {
      std::cerr << temp.native() << " \n";

      database db;
      boost::any cfg = mira::utilities::default_database_configuration();

      db.add_index< book_index >();
      BOOST_CHECK_THROW( db.add_index<book_index>(), std::logic_error ); /// cannot add same index twice

      db.open( temp, 0, cfg );

      BOOST_TEST_MESSAGE( "Creating book" );
      const auto& new_book = db.create<book>( []( book& b ) {
          b.a = 3;
          b.b = 4;
      } );
      const auto& copy_new_book = db.get( book::id_type(0) );

      // BOOST_REQUIRE( &new_book != &copy_new_book ); ///< the same cached copy should be returned
      BOOST_REQUIRE( &db.get( book::id_type(0) ) == &db.get( book::id_type(0) ) );

      BOOST_REQUIRE_EQUAL( new_book.a, copy_new_book.a );
      BOOST_REQUIRE_EQUAL( new_book.b, copy_new_book.b );

      db.modify( new_book, [&]( book& b ) {
          b.a = 5;
          b.b = 6;
      });
      BOOST_REQUIRE_EQUAL( new_book.a, 5 );
      BOOST_REQUIRE_EQUAL( new_book.b, 6 );

      BOOST_REQUIRE_EQUAL( new_book.a, copy_new_book.a );
      BOOST_REQUIRE_EQUAL( new_book.b, copy_new_book.b );

      {
         auto session = db.start_undo_session();
         db.modify( new_book, [&]( book& b ) {
            b.a = 7;
            b.b = 8;
         });

         const auto& new_book = db.get( book::id_type( 0 ) );

         BOOST_REQUIRE_EQUAL( new_book.a, 7 );
         BOOST_REQUIRE_EQUAL( new_book.b, 8 );
      }

      {
         const auto& new_book = db.get( book::id_type( 0 ) );
         BOOST_REQUIRE_EQUAL( new_book.a, 5 );
         BOOST_REQUIRE_EQUAL( new_book.b, 6 );
      }

      {
          auto session = db.start_undo_session();
          const auto& book2 = db.create<book>( [&]( book& b ) {
              b.a = 9;
              b.b = 10;
          });

         BOOST_REQUIRE_EQUAL( new_book.a, 5 );
         BOOST_REQUIRE_EQUAL( new_book.b, 6 );
         BOOST_REQUIRE_EQUAL( book2.a, 9 );
         BOOST_REQUIRE_EQUAL( book2.b, 10 );
      }

      BOOST_CHECK_THROW( db.get( book::id_type(1) ), std::out_of_range );
      BOOST_REQUIRE_EQUAL( new_book.a, 5 );
      BOOST_REQUIRE_EQUAL( new_book.b, 6 );

      {
          auto session = db.start_undo_session();
          db.modify( new_book, [&]( book& b ) {
              b.a = 7;
              b.b = 8;
          });

         const auto& new_book = db.get( book::id_type(0) );
         BOOST_REQUIRE_EQUAL( new_book.a, 7 );
         BOOST_REQUIRE_EQUAL( new_book.b, 8 );
         session.push();
      }

      {
         const auto& new_book = db.get( book::id_type(0) );
         BOOST_REQUIRE_EQUAL( new_book.a, 7 );
         BOOST_REQUIRE_EQUAL( new_book.b, 8 );
      }

      db.undo();
      {
         const auto& new_book = db.get( book::id_type(0) );
         BOOST_REQUIRE_EQUAL( new_book.a, 5 );
         BOOST_REQUIRE_EQUAL( new_book.b, 6 );

         BOOST_REQUIRE_EQUAL( new_book.a, copy_new_book.a );
         BOOST_REQUIRE_EQUAL( new_book.b, copy_new_book.b );
      }

      {
          auto session = db.start_undo_session();
          db.modify( new_book, [&]( book& b ) {
              b.a = 7;
              b.b = 8;
          });

         const auto& new_book = db.get( book::id_type(0) );
         BOOST_REQUIRE_EQUAL( new_book.a, 7 );
         BOOST_REQUIRE_EQUAL( new_book.b, 8 );
         session.push();
      }

      db.commit( db.revision() );
      {
         const auto& new_book = db.get( book::id_type(0) );
         BOOST_REQUIRE_EQUAL( new_book.a, 7 );
         BOOST_REQUIRE_EQUAL( new_book.b, 8 );
      }
   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( merge_iterator )
{ try {
   boost::filesystem::path temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();

   database db;
   boost::any cfg = mira::utilities::default_database_configuration();

   db.add_index< book_index >();
   db.open( temp, 0, cfg );

   // Book 0: a: 5, b: 10, sum: 15
   // Book 1: a: 1, b: 7, sum: 8
   // Book 2: a: 10, b:3, sum 13
   db.create< book >( [&]( book& b )
   {
      b.a = 5;
      b.b = 10;
   });

   db.create< book >( [&]( book& b )
   {
      b.a = 1;
      b.b = 7;
   });

   db.create< book >( [&]( book& b )
   {
      b.a = 10;
      b.b = 3;
   });

   // Undo State 0 orders:
   // by_a: 1, 0, 2
   // by_b: 2, 1, 0
   // by_sum: 1, 2, 0
   {
      const auto& by_id_idx = db.get_index< book_index, by_id >();
      auto id_itr = by_id_idx.begin();

      BOOST_REQUIRE( id_itr != by_id_idx.end() );
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 5 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 10 );
      ++id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 7 );
      ++id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );
      ++id_itr;
      BOOST_REQUIRE( id_itr == by_id_idx.end() );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 7 );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 5 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 10 );

      auto id_ptr = db.find< book, by_id >( book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_ptr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_ptr->a, 1 );
      BOOST_REQUIRE_EQUAL( id_ptr->b, 7 );

      const auto& by_a_idx = db.get_index< book_index, by_a >();
      auto a_itr = by_a_idx.begin();

      BOOST_REQUIRE( a_itr != by_a_idx.end() );
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 7 );
      ++a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 5 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 10 );
      ++a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 3 );
      ++a_itr;
      BOOST_REQUIRE( a_itr == by_a_idx.end() );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 3 );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 5 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 10 );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 7 );

      const auto& by_b_idx = db.get_index< book_index, by_b >();
      auto b_itr = by_b_idx.begin();

      BOOST_REQUIRE( b_itr != by_b_idx.end() );
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 3 );
      ++b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 7 );
      ++b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 5 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 10 );
      ++b_itr;
      BOOST_REQUIRE( b_itr == by_b_idx.end() );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 5 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 10 );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 7 );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 3 );

      const auto& by_sum_idx = db.get_index< book_index, by_sum >();
      auto sum_itr = by_sum_idx.begin();

      BOOST_REQUIRE( sum_itr != by_sum_idx.end() );
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 7 );
      ++sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 3 );
      ++sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 5 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 10 );
      ++sum_itr;
      BOOST_REQUIRE( sum_itr == by_sum_idx.end() );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 5 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 10 );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 3 );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 7 );
   }

   // Book 0: a: 2, b: 13, sum: 15
   // Book 1: a: 3, b: 5, sum: 8
   // Book 2: a: 10, b: 3, sum: 13 (not changed)
   {
      auto undo_session_1 = db.start_undo_session();
      const auto& book_0 = db.get( book::id_type(0) );
      BOOST_REQUIRE_EQUAL( book_0.id._id, 0 );
      BOOST_REQUIRE_EQUAL( book_0.a, 5 );
      BOOST_REQUIRE_EQUAL( book_0.b, 10 );
      db.modify( book_0, [&]( book& b )
      {
         b.a = 2;
         b.b = 13;
      });

      const auto& book_1 = db.get( book::id_type(1) );
      BOOST_REQUIRE_EQUAL( book_1.id._id, 1 );
      BOOST_REQUIRE_EQUAL( book_1.a, 1 );
      BOOST_REQUIRE_EQUAL( book_1.b, 7 );
      db.modify( db.get( book::id_type(1) ), [&]( book& b )
      {
         b.a = 3;
         b.b = 5;
      });
      undo_session_1.push();
   }

   // Undo State 1 orders:
   // by_a: 0, 1, 2
   // by_b: 2, 1, 0 (not changed)
   // by_sum: 1, 2, 0 (not changed)
   {
      const auto& by_id_idx = db.get_index< book_index, by_id >();
      auto id_itr = by_id_idx.begin();

      BOOST_REQUIRE( id_itr != by_id_idx.end() );
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 13 );
      ++id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 3 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 5 );
      ++id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );
      ++id_itr;
      BOOST_REQUIRE( id_itr == by_id_idx.end() );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 3 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 5 );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 13 );

      auto id_ptr = db.find< book, by_id >( book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_ptr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_ptr->a, 3 );
      BOOST_REQUIRE_EQUAL( id_ptr->b, 5 );

      const auto& by_a_idx = db.get_index< book_index, by_a >();
      auto a_itr = by_a_idx.begin();

      BOOST_REQUIRE( a_itr != by_a_idx.end() );
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 13 );
      ++a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 3 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 5 );
      ++a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 3 );
      ++a_itr;
      BOOST_REQUIRE( a_itr == by_a_idx.end() );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 3 );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 3 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 5 );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 13 );

      const auto& by_b_idx = db.get_index< book_index, by_b >();
      auto b_itr = by_b_idx.begin();

      BOOST_REQUIRE( b_itr != by_b_idx.end() );
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 3 );
      ++b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 3 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 5 );
      ++b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 13 );
      ++b_itr;
      BOOST_REQUIRE( b_itr == by_b_idx.end() );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 13 );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 3 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 5 );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 3 );

      const auto& by_sum_idx = db.get_index< book_index, by_sum >();
      auto sum_itr = by_sum_idx.begin();

      BOOST_REQUIRE( sum_itr != by_sum_idx.end() );
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 3 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 5 );
      ++sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 3 );
      ++sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 13 );
      ++sum_itr;
      BOOST_REQUIRE( sum_itr == by_sum_idx.end() );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 13 );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 3 );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 3 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 5 );
   }

   // Book 0: a: 2, b: 13, sum: 15 (not changed)
   // Book 1: a: 1, b: 20, sum: 21
   // Book 2: a: 10, b: 3, sum: 13 (not changed)
   {
      auto undo_session_2 = db.start_undo_session();
      db.modify( db.get( book::id_type(1) ), [&]( book& b )
      {
         b.a = 1;
         b.b = 20;
      });
      undo_session_2.push();
   }

   // Undo State 2 orders:
   // by_a: 1, 0, 2
   // by_b: 2, 0, 1
   // by_sum: 2, 0, 1
   {
      const auto& by_id_idx = db.get_index< book_index, by_id >();
      auto id_itr = by_id_idx.begin();

      BOOST_REQUIRE( id_itr != by_id_idx.end() );
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 13 );
      ++id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 20 );
      ++id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );
      ++id_itr;
      BOOST_REQUIRE( id_itr == by_id_idx.end() );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 20 );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 13 );

      auto id_ptr = db.find< book, by_id >( book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_ptr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_ptr->a, 1 );
      BOOST_REQUIRE_EQUAL( id_ptr->b, 20 );

      const auto& by_a_idx = db.get_index< book_index, by_a >();
      auto a_itr = by_a_idx.begin();

      BOOST_REQUIRE( a_itr != by_a_idx.end() );
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 20 );
      ++a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 13 );
      ++a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 3 );
      ++a_itr;
      BOOST_REQUIRE( a_itr == by_a_idx.end() );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 3 );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 13 );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 20 );

      const auto& by_b_idx = db.get_index< book_index, by_b >();
      auto b_itr = by_b_idx.begin();

      BOOST_REQUIRE( b_itr != by_b_idx.end() );
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 3 );
      ++b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 13 );
      ++b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 20 );
      ++b_itr;
      BOOST_REQUIRE( b_itr == by_b_idx.end() );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 20 );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 13 );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 3 );

      const auto& by_sum_idx = db.get_index< book_index, by_sum >();
      auto sum_itr = by_sum_idx.begin();

      BOOST_REQUIRE( sum_itr != by_sum_idx.end() );
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 3 );
      ++sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 13 );
      ++sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 20 );
      ++sum_itr;
      BOOST_REQUIRE( sum_itr == by_sum_idx.end() );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 20 );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 13 );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 3 );
   }

   // Book: 0 (removed)
   // Book 1: a: 1, b: 20, sum: 21 (not changed)
   // Book 2: a: 10, b: 3, sum: 13 (not changed)
   {
      auto undo_session_3 = db.start_undo_session();
      db.remove( db.get( book::id_type(0) ) );
      undo_session_3.push();
   }

   // Undo State 3 orders:
   // by_a: 1, 2
   // by_b: 2, 1
   // by_sum: 2, 1
   {
      const auto& by_id_idx = db.get_index< book_index, by_id >();
      auto id_itr = by_id_idx.begin();

      BOOST_REQUIRE( id_itr != by_id_idx.end() );
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 20 );
      ++id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );
      ++id_itr;
      BOOST_REQUIRE( id_itr == by_id_idx.end() );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 20 );

      auto id_ptr = db.find< book, by_id >( book::id_type( 0 ) );
      BOOST_REQUIRE_EQUAL( id_ptr, nullptr );

      const auto& by_a_idx = db.get_index< book_index, by_a >();
      auto a_itr = by_a_idx.begin();

      BOOST_REQUIRE( a_itr != by_a_idx.end() );
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 20 );
      ++a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 3 );
      ++a_itr;
      BOOST_REQUIRE( a_itr == by_a_idx.end() );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 3 );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 20 );

      const auto& by_b_idx = db.get_index< book_index, by_b >();
      auto b_itr = by_b_idx.begin();

      BOOST_REQUIRE( b_itr != by_b_idx.end() );
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 3 );
      ++b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 20 );
      ++b_itr;
      BOOST_REQUIRE( b_itr == by_b_idx.end() );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 20 );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 3 );

      const auto& by_sum_idx = db.get_index< book_index, by_sum >();
      auto sum_itr = by_sum_idx.begin();

      BOOST_REQUIRE( sum_itr != by_sum_idx.end() );
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 3 );
      ++sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 20 );
      ++sum_itr;
      BOOST_REQUIRE( sum_itr == by_sum_idx.end() );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 20 );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 3 );
   }

   // Book 1: a: 1, b: 20, sum: 21 (not changed)
   // Book 2: a: 10, b: 3, sum: 13 (not changed)
   // Book 3: a: 2, b: 13, sum: 15 (old book 0)
   {
      auto undo_session_4 = db.start_undo_session();
      db.create< book >( [&]( book& b )
      {
         b.a = 2;
         b.b = 13;
      });
      undo_session_4.push();
   }

   // Undo State 4 orders:
   // by_a: 1, 3, 2
   // by_b: 2, 3, 1
   // by_sum: 2, 3, 1
   {
      const auto& by_id_idx = db.get_index< book_index, by_id >();
      auto id_itr = by_id_idx.begin();

      BOOST_REQUIRE( id_itr != by_id_idx.end() );
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 20 );
      ++id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );
      ++id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 13 );
      ++id_itr;
      BOOST_REQUIRE( id_itr == by_id_idx.end() );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 13 );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 20 );

      auto id_ptr = db.find< book, by_id >( book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( id_ptr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( id_ptr->a, 2 );
      BOOST_REQUIRE_EQUAL( id_ptr->b, 13 );

      const auto& by_a_idx = db.get_index< book_index, by_a >();
      auto a_itr = by_a_idx.begin();

      BOOST_REQUIRE( a_itr != by_a_idx.end() );
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 20 );
      ++a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 13 );
      ++a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 3 );
      ++a_itr;
      BOOST_REQUIRE( a_itr == by_a_idx.end() );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 3 );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 13 );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 20 );

      const auto& by_b_idx = db.get_index< book_index, by_b >();
      auto b_itr = by_b_idx.begin();

      BOOST_REQUIRE( b_itr != by_b_idx.end() );
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 3 );
      ++b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 13 );
      ++b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 20 );
      ++b_itr;
      BOOST_REQUIRE( b_itr == by_b_idx.end() );

      const auto& by_sum_idx = db.get_index< book_index, by_sum >();
      auto sum_itr = by_sum_idx.begin();

      BOOST_REQUIRE( sum_itr != by_sum_idx.end() );
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 3 );
      ++sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 13 );
      ++sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 20 );
      ++sum_itr;
      BOOST_REQUIRE( sum_itr == by_sum_idx.end() );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 20 );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 13 );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 3 );
   }

   db.commit( 2 );
   {
      const auto& by_id_idx = db.get_index< book_index, by_id >();
      auto id_itr = by_id_idx.begin();

      BOOST_REQUIRE( id_itr != by_id_idx.end() );
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 20 );
      ++id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );
      ++id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 13 );
      ++id_itr;
      BOOST_REQUIRE( id_itr == by_id_idx.end() );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 13 );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 20 );
      ++id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 20 );
      ++id_itr;
      ++id_itr;
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );

      const auto& by_a_idx = db.get_index< book_index, by_a >();
      auto a_itr = by_a_idx.begin();

      BOOST_REQUIRE( a_itr != by_a_idx.end() );
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 20 );
      ++a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 13 );
      ++a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 3 );
      ++a_itr;
      BOOST_REQUIRE( a_itr == by_a_idx.end() );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 3 );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 13 );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 20 );

      const auto& by_b_idx = db.get_index< book_index, by_b >();
      auto b_itr = by_b_idx.begin();

      BOOST_REQUIRE( b_itr != by_b_idx.end() );
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 3 );
      ++b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 13 );
      ++b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 20 );
      ++b_itr;
      BOOST_REQUIRE( b_itr == by_b_idx.end() );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 20 );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 13 );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 3 );

      const auto& by_sum_idx = db.get_index< book_index, by_sum >();
      auto sum_itr = by_sum_idx.begin();

      BOOST_REQUIRE( sum_itr != by_sum_idx.end() );
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 3 );
      ++sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 13 );
      ++sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 20 );
      ++sum_itr;
      BOOST_REQUIRE( sum_itr == by_sum_idx.end() );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 20 );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 13 );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 3 );
   }

   for( size_t i = 3; i <= db.revision(); i++ )
   {
      db.commit( i );

      const auto& by_id_idx = db.get_index< book_index, by_id >();
      auto id_itr = by_id_idx.begin();

      BOOST_REQUIRE( id_itr != by_id_idx.end() );
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 20 );
      ++id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );
      ++id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 13 );
      ++id_itr;
      BOOST_REQUIRE( id_itr == by_id_idx.end() );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 13 );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 20 );
      ++id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 20 );
      ++id_itr;
      ++id_itr;
      --id_itr;
      BOOST_REQUIRE_EQUAL( id_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( id_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( id_itr->b, 3 );

      const auto& by_a_idx = db.get_index< book_index, by_a >();
      auto a_itr = by_a_idx.begin();

      BOOST_REQUIRE( a_itr != by_a_idx.end() );
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 20 );
      ++a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 13 );
      ++a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 3 );
      ++a_itr;
      BOOST_REQUIRE( a_itr == by_a_idx.end() );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 3 );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 13 );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 20 );
      ++a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 13 );
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 20 );
      ++a_itr;
      ++a_itr;
      --a_itr;
      BOOST_REQUIRE_EQUAL( a_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( a_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( a_itr->b, 13 );

      const auto& by_b_idx = db.get_index< book_index, by_b >();
      auto b_itr = by_b_idx.begin();

      BOOST_REQUIRE( b_itr != by_b_idx.end() );
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 3 );
      ++b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 13 );
      ++b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 20 );
      ++b_itr;
      BOOST_REQUIRE( b_itr == by_b_idx.end() );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 20 );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 13 );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 3 );
      ++b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 13 );
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 3 );
      ++b_itr;
      ++b_itr;
      --b_itr;
      BOOST_REQUIRE_EQUAL( b_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( b_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( b_itr->b, 13 );

      const auto& by_sum_idx = db.get_index< book_index, by_sum >();
      auto sum_itr = by_sum_idx.begin();

      BOOST_REQUIRE( sum_itr != by_sum_idx.end() );
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 3 );
      ++sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 13 );
      ++sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 20 );
      ++sum_itr;
      BOOST_REQUIRE( sum_itr == by_sum_idx.end() );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 1 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 1 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 20 );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 13 );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 3 );
      ++sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 13 );
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 2 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 10 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 3 );
      ++sum_itr;
      ++sum_itr;
      --sum_itr;
      BOOST_REQUIRE_EQUAL( sum_itr->id, book::id_type( 3 ) );
      BOOST_REQUIRE_EQUAL( sum_itr->a, 2 );
      BOOST_REQUIRE_EQUAL( sum_itr->b, 13 );
   }
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( key_uniqueness )
{ try {
   boost::filesystem::path temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();

   database db;
   boost::any cfg = mira::utilities::default_database_configuration();

   db.add_index< book_index >();
   db.open( temp, 0, cfg );

   // Undo 0: {"id": 0, "a": 2, "b": 2}
   // Undo 1: Add {"id": 1, "a": 1, "b": 4}
   // In Undo 2: Test adding conflicting key {"a": 2}
   // In Undo 2: Test adding conflicting key {"b": 4}
   // Undo 2: Remove {"id": 0}
   // Undo 3: Add {"id": 2, "a": 2, "b": 1}

   db.create< book >( [&]( book& b )
   {
      b.a = 2;
      b.b = 2;
   });

   {
      auto undo_session_1 = db.start_undo_session();
      db.create< book >( [&]( book& b )
      {
         b.a = 1;
         b.b = 4;
      });
      undo_session_1.push();
   }

   {
      auto undo_session_2 = db.start_undo_session();
      try
      {
         db.create< book >( [&]( book& b )
         {
            b.a = 2;
            b.b = 5;
         });
         BOOST_FAIL( "std::logic_error was not thrown" );
      }
      catch( std::logic_error& ) { /* success */ }

      try
      {
         db.create< book >( [&]( book& b )
         {
            b.a = 3;
            b.b = 2;
         });
         BOOST_FAIL( "std::logic_error was not thrown" );
      }
      catch( std::logic_error& ) { /* success */ }

      db.remove( db.get( book::id_type( 0 ) ) );

      undo_session_2.push();
   }

   {
      auto undo_session_3 = db.start_undo_session();
      db.create< book >( [&]( book& b )
      {
         b.a = 2;
         b.b = 1;
      });
      undo_session_3.push();
   }


} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
