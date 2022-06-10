#pragma once

#define KOINOS_CHECK_THROW( S, C )                                      \
do                                                                      \
{                                                                       \
   try                                                                  \
   {                                                                    \
      S;                                                                \
      BOOST_TEST(false, "koinos::exception not thrown when expected" ); \
   }                                                                    \
   catch ( const koinos::exception& e )                                 \
   {                                                                    \
      BOOST_TEST_REQUIRE( e.get_code() == C, "exception code is not " #C ", was " + std::to_string( e.get_code() ) ); \
   }                                                                    \
} while( 0 );

#define KOINOS_REQUIRE_THROW( S, C )                                             \
do                                                                               \
{                                                                                \
   try                                                                           \
   {                                                                             \
      S;                                                                         \
      BOOST_TEST_REQUIRE(false, "koinos::exception not thrown when expected" );  \
   }                                                                             \
   catch ( const koinos::exception& e )                                          \
   {                                                                             \
      BOOST_TEST_REQUIRE( e.get_code() == C, "exception code is not " #C ", was " + std::to_string( e.get_code() ) ); \
   }                                                                             \
} while( 0 );
