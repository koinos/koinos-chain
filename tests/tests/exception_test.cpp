#include <boost/test/unit_test.hpp>

#include <koinos/exception.hpp>
#include <koinos/pack/rt/reflect.hpp>

#include <iostream>

struct exception_test_object
{
   uint32_t x = 0;
   uint32_t y = 0;
};

KOINOS_REFLECT( exception_test_object, (x)(y) )

struct exception_fixture {};

KOINOS_DECLARE_EXCEPTION( my_exception );

BOOST_FIXTURE_TEST_SUITE( exception_tests, exception_fixture )

BOOST_AUTO_TEST_CASE( exception_test )
{ try {
   nlohmann::json exception_json;
   exception_json["x"] = "foo";
   exception_json["y"] = "bar";

   BOOST_TEST_MESSAGE( "Throw an exception with an initial capture and a caught capture." );
   try
   {
      try
      {
         KOINOS_THROW( my_exception, "exception_test ${x} ${y}", ("x","foo") );
      }
      KOINOS_CAPTURE_CATCH_AND_RETHROW( ("y","bar") )
   }
   catch( koinos::exception& e )
   {
      auto j = e.get_json();
      BOOST_REQUIRE_EQUAL( exception_json, j );
      BOOST_REQUIRE_EQUAL( e.get_message(), "exception_test foo bar" );
      BOOST_REQUIRE_EQUAL( e.what(), e.get_message() );
   }

   BOOST_TEST_MESSAGE( "Throw an exception with no initial capture and a caught capture." );
   try
   {
      try
      {
         KOINOS_THROW( my_exception, "exception_test ${x} ${y}" );
      }
      KOINOS_CAPTURE_CATCH_AND_RETHROW( ("y","bar")("x","foo") )
   }
   catch( koinos::exception& e )
   {
      auto j = e.get_json();
      BOOST_REQUIRE_EQUAL( exception_json, j );
      BOOST_REQUIRE_EQUAL( e.get_message(), "exception_test foo bar" );
      BOOST_REQUIRE_EQUAL( e.what(), e.get_message() );
   }

   BOOST_TEST_MESSAGE( "Throw an exception with an initial capture and a caught extra capture." );
   try
   {
      try
      {
         KOINOS_THROW( my_exception, "exception_test ${x} ${y}", ("y","bar")("x","foo") );
      }
      KOINOS_CAPTURE_CATCH_AND_RETHROW( ("z",10) )
   }
   catch( koinos::exception& e )
   {
      exception_json["z"] = 10;
      auto j = e.get_json();
      BOOST_REQUIRE_EQUAL( exception_json, j );
      BOOST_REQUIRE_EQUAL( e.get_message(), "exception_test foo bar" );
      BOOST_REQUIRE_EQUAL( e.what(), e.get_message() );
   }

   BOOST_TEST_MESSAGE( "Throw an exception with an initial object capture and a missing capture." );
   try
   {
      try
      {
         exception_test_object obj = {1,2};
         KOINOS_THROW( my_exception, "exception_test ${x} ${y}", ("x", obj) );
      }
      KOINOS_CAPTURE_CATCH_AND_RETHROW( ("z",exception_test_object{3,4}) )
   }
   catch( koinos::exception& e )
   {
      exception_json.clear();
      exception_json["x"]["x"] = 1;
      exception_json["x"]["y"] = 2;
      exception_json["z"]["x"] = 3;
      exception_json["z"]["y"] = 4;
      auto j = e.get_json();
      BOOST_REQUIRE_EQUAL( exception_json, j );
      BOOST_REQUIRE_EQUAL( e.get_message(), "exception_test {\"x\":1,\"y\":2} ${y}" );
   }

   BOOST_TEST_MESSAGE( "Throw an exception with an initial implicit object capture." );
   try
   {
      exception_test_object obj = {1,2};
      KOINOS_THROW( my_exception, "exception_test ${x} ${y}", (obj) );
   }
   catch( koinos::exception& e )
   {
      exception_json.clear();
      exception_json["x"] = 1;
      exception_json["y"] = 2;
      BOOST_REQUIRE_EQUAL( e.get_message(), "exception_test 1 2" );
      BOOST_REQUIRE_EQUAL( e.get_message(), e.what() );
   }

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_SUITE_END()
