#include <boost/test/unit_test.hpp>

#include <koinos/tests/net_fixture.hpp>

#include <koinos/exception.hpp>
#include <koinos/net/protocol/jsonrpc/request_handler.hpp>
#include <koinos/net/protocol/jsonrpc/types.hpp>

BOOST_FIXTURE_TEST_SUITE( net_tests, net_fixture )

using namespace boost::beast;
using namespace koinos::net::protocol;
using json = jsonrpc::json;

BOOST_AUTO_TEST_CASE( http_server_tests )
{ try {
   {
      BOOST_TEST_MESSAGE( "send an unsupported http method" );
      http::request< http::string_body > req { http::verb::delete_, "/", 11 };
      req.set( http::field::host, "127.0.0.1" );
      req.set( http::field::user_agent, "koinos_tests/1.0" );
      req.set( http::field::content_type, "text/html" );
      req.keep_alive( true );
      req.prepare_payload();
      http::write( *socket, req );

      auto resp = read_http();

      BOOST_TEST_MESSAGE( "-> verifying result" );
      BOOST_REQUIRE( resp.result_int() == uint64_t( http::status::bad_request ) );
      BOOST_REQUIRE( resp.body() == "unsupported http method" );
   }

   {
      BOOST_TEST_MESSAGE( "send an unsupported http target" );
      http::request< http::string_body > req { http::verb::post, "/unknown", 11 };
      req.set( http::field::host, "127.0.0.1" );
      req.set( http::field::user_agent, "koinos_tests/1.0" );
      req.set( http::field::content_type, "text/html" );
      req.keep_alive( true );
      req.prepare_payload();
      http::write( *socket, req );

      auto resp = read_http();

      BOOST_TEST_MESSAGE( "-> verifying result" );
      BOOST_REQUIRE( resp.result_int() == uint64_t( http::status::not_found ) );
      BOOST_REQUIRE( resp.body() == "unsupported target" );
   }

   {
      BOOST_TEST_MESSAGE( "send an unsupported content type" );

      http::request< http::string_body > req { http::verb::post, "/", 11 };
      req.set( http::field::host, "127.0.0.1" );
      req.set( http::field::user_agent, "koinos_tests/1.0" );
      req.set( http::field::content_type, "text/html" );
      req.keep_alive( true );
      req.prepare_payload();
      http::write( *socket, req );

      auto resp = read_http();

      BOOST_TEST_MESSAGE( "-> verifying result" );
      BOOST_REQUIRE( resp.result_int() == uint64_t( http::status::internal_server_error ) );
      BOOST_REQUIRE( resp.body() == "unsupported content-type" );
   }

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( jsonrpc_server_tests )
{ try {

   BOOST_TEST_MESSAGE( "adding method handlers [add, sub, mul, div]" );

   auto request_handler = std::make_shared< jsonrpc::request_handler >();
   http_router->handlers[ "application/json" ] = request_handler;

   request_handler->add_method_handler( "add", []( const json::object_t& j ) -> json
   {
      if ( !j.count( "a" ) || !j.count( "b" ) || !j.at( "a" ).is_number() || !j.at( "b" ).is_number() )
         throw jsonrpc::exception( jsonrpc::error_code::invalid_params, "invalid params", "\"a\" and \"b\" must exist as numbers" );

      json result = j.at( "a" ).get< uint64_t >() + j.at( "b" ).get< uint64_t >();
      return result;
   } );

   request_handler->add_method_handler( "sub", []( const json::object_t& j ) -> json
   {
      if ( !j.count( "a" ) || !j.count( "b" ) || !j.at( "a" ).is_number() || !j.at( "b" ).is_number() )
         throw jsonrpc::exception( jsonrpc::error_code::invalid_params, "invalid params", "\"a\" and \"b\" must exist as numbers" );

      json result = j.at( "a" ).get< uint64_t >() - j.at( "b" ).get< uint64_t >();
      return result;
   } );

   request_handler->add_method_handler( "mul", []( const json::object_t& j ) -> json
   {
      if ( !j.count( "a" ) || !j.count( "b" ) || !j.at( "a" ).is_number() || !j.at( "b" ).is_number() )
         throw jsonrpc::exception( jsonrpc::error_code::invalid_params, "invalid params", "\"a\" and \"b\" must exist as numbers" );

      json result = j.at( "a" ).get< uint64_t >() * j.at( "b" ).get< uint64_t >();
      return result;
   } );

   request_handler->add_method_handler( "div", []( const json::object_t& j ) -> json
   {
      if ( !j.count( "a" ) || !j.count( "b" ) || !j.at( "a" ).is_number() || !j.at( "b" ).is_number() )
         throw jsonrpc::exception( jsonrpc::error_code::invalid_params, "invalid params", "\"a\" and \"b\" must exist as numbers" );

      if ( j.at( "b" ).get< uint64_t >() == 0 )
         throw std::runtime_error( "cannot divide by zero" );

      json result = j.at( "a" ).get< uint64_t >() / j.at( "b" ).get< uint64_t >();
      return result;
   } );

   BOOST_TEST_MESSAGE( "adding duplicate method handler 'div'" );

   BOOST_REQUIRE_THROW(
      request_handler->add_method_handler( "div", []( const json::object_t& j ) -> json { return json(); } ),
      std::runtime_error
   );

   BOOST_TEST_MESSAGE( "sending 'add' request with params {\"a\":2, \"b\":1}" );

   jsonrpc::request req {
      .jsonrpc = "2.0",
      .id = uint64_t( 1 ),
      .method = "add",
      .params = json::object_t {
         { "a", 2 },
         { "b", 1 }
      }
   };

   write_request( req );

   jsonrpc::response res = read_response();

   BOOST_TEST_MESSAGE( "-> verifying result" );

   BOOST_REQUIRE_EQUAL( res.jsonrpc, "2.0" );
   BOOST_REQUIRE_EQUAL( std::get< uint64_t >( res.id ), 1 );
   BOOST_REQUIRE_EQUAL( res.result->get< uint64_t >(), 3 );
   BOOST_REQUIRE( !res.error.has_value() );

   BOOST_TEST_MESSAGE( "sending 'sub' request with params {\"a\":2, \"b\":1}" );

   req.id = uint64_t( 2 );
   req.method = "sub";

   write_request( req );

   res = read_response();

   BOOST_TEST_MESSAGE( "-> verifying result" );

   BOOST_REQUIRE_EQUAL( res.jsonrpc, "2.0" );
   BOOST_REQUIRE_EQUAL( std::get< uint64_t >( res.id ), 2 );
   BOOST_REQUIRE_EQUAL( res.result->get< uint64_t >(), 1 );
   BOOST_REQUIRE( !res.error.has_value() );

   BOOST_TEST_MESSAGE( "sending 'mul' request with params {\"a\":5, \"b\":6}" );

   req.id = uint64_t( 3 );
   req.method = "mul";
   req.params = json::object_t {
      { "a", 5 },
      { "b", 6 }
   };

   write_request( req );

   res = read_response();

   BOOST_TEST_MESSAGE( "-> verifying result" );

   BOOST_REQUIRE_EQUAL( res.jsonrpc, "2.0" );
   BOOST_REQUIRE_EQUAL( std::get< uint64_t >( res.id ), 3 );
   BOOST_REQUIRE_EQUAL( res.result->get< uint64_t >(), 30 );
   BOOST_REQUIRE( !res.error.has_value() );

   BOOST_TEST_MESSAGE( "sending 'div' request with params {\"a\":100, \"b\":5}" );

   req.id = uint64_t( 4 );
   req.method = "div";
   req.params = json::object_t {
      { "a", 100 },
      { "b", 5 }
   };

   write_request( req );

   res = read_response();

   BOOST_TEST_MESSAGE( "-> verifying result" );

   BOOST_REQUIRE_EQUAL( res.jsonrpc, "2.0" );
   BOOST_REQUIRE_EQUAL( std::get< uint64_t >( res.id ), 4 );
   BOOST_REQUIRE_EQUAL( res.result->get< uint64_t >(), 20 );
   BOOST_REQUIRE( !res.error.has_value() );

   BOOST_TEST_MESSAGE( "sending request that has an unhandled method" );

   req.id = uint64_t( 5 );
   req.method = "unknown";
   req.params = json::object_t {
      { "a", 100 },
      { "b", 5 }
   };

   write_request( req );

   res = read_response();

   BOOST_TEST_MESSAGE( "-> verifying result" );
   BOOST_REQUIRE_EQUAL( res.jsonrpc, "2.0" );
   BOOST_REQUIRE_EQUAL( std::get< uint64_t >( res.id ), 5 );
   BOOST_REQUIRE( !res.result.has_value() );
   BOOST_REQUIRE( res.error->code == jsonrpc::error_code::method_not_found );
   BOOST_REQUIRE( res.error->message == "method not found: unknown" );
   BOOST_REQUIRE( !res.error->data.has_value() );

   BOOST_TEST_MESSAGE( "sending request that an invalid json rpc version" );

   req.id = uint64_t( 6 );
   req.jsonrpc = "2.1";
   req.method = "add";
   req.params = json::object_t {
      { "a", 100 },
      { "b", 5 }
   };

   write_request( req );

   res = read_response();

   BOOST_TEST_MESSAGE( "-> verifying result" );
   BOOST_REQUIRE_EQUAL( res.jsonrpc, "2.0" );
   BOOST_REQUIRE_EQUAL( std::get< uint64_t >( res.id ), 6 );
   BOOST_REQUIRE( !res.result.has_value() );
   BOOST_REQUIRE( res.error->code == jsonrpc::error_code::invalid_request );
   BOOST_REQUIRE( res.error->message == "an invalid jsonrpc version was provided" );
   BOOST_REQUIRE( !res.error->data.has_value() );

   BOOST_TEST_MESSAGE( "sending request that has a fractional id" );

   write_http( "{ \"jsonrpc\": \"2.0\", \"id\": 1.1, \"method\": \"add\", \"params\": { \"a\": 1, \"b\": 2 } }" );

   res = read_response();

   BOOST_TEST_MESSAGE( "-> verifying result" );
   BOOST_REQUIRE_EQUAL( res.jsonrpc, "2.0" );
   BOOST_REQUIRE( std::get< std::nullptr_t >( res.id ) == nullptr );
   BOOST_REQUIRE( !res.result.has_value() );
   BOOST_REQUIRE( res.error->code == jsonrpc::error_code::invalid_request );
   BOOST_REQUIRE( res.error->message == "id cannot be fractional" );
   BOOST_REQUIRE( !res.error->data.has_value() );

   BOOST_TEST_MESSAGE( "sending request that has invalid id type" );

   write_http( "{ \"jsonrpc\": \"2.0\", \"id\": [1], \"method\": \"add\", \"params\": { \"a\": 1, \"b\": 2 } }" );

   res = read_response();

   BOOST_TEST_MESSAGE( "-> verifying result" );
   BOOST_REQUIRE_EQUAL( res.jsonrpc, "2.0" );
   BOOST_REQUIRE( std::get< std::nullptr_t >( res.id ) == nullptr );
   BOOST_REQUIRE( !res.result.has_value() );
   BOOST_REQUIRE( res.error->code == jsonrpc::error_code::invalid_request );
   BOOST_REQUIRE( res.error->message == "id must be a non-fractional number, string or null" );
   BOOST_REQUIRE( !res.error->data.has_value() );

   BOOST_TEST_MESSAGE( "sending request that has invalid params" );

   write_http( "{ \"jsonrpc\": \"2.0\", \"id\": 189, \"method\": \"add\", \"params\": { \"a\": \"1\", \"b\": 2 } }" );

   res = read_response();

   BOOST_TEST_MESSAGE( "-> verifying result" );
   BOOST_REQUIRE_EQUAL( res.jsonrpc, "2.0" );
   BOOST_REQUIRE( std::get< uint64_t >( res.id ) == 189 );
   BOOST_REQUIRE( !res.result.has_value() );
   BOOST_REQUIRE( res.error->code == jsonrpc::error_code::invalid_params );
   BOOST_REQUIRE( res.error->message == "invalid params" );
   BOOST_REQUIRE( res.error->data.value() == "\"a\" and \"b\" must exist as numbers"  );

   BOOST_TEST_MESSAGE( "sending request that throws a server error" );

   req.id = uint64_t( 65 );
   req.method = "div";
   req.jsonrpc = "2.0";
   req.params = json::object_t {
      { "a", 100 },
      { "b", 0 }
   };

   write_request( req );

   res = read_response();

   BOOST_TEST_MESSAGE( "-> verifying result" );
   BOOST_REQUIRE_EQUAL( res.jsonrpc, "2.0" );
   BOOST_REQUIRE( std::get< uint64_t >( res.id ) == 65 );
   BOOST_REQUIRE( !res.result.has_value() );
   BOOST_REQUIRE( res.error->code == jsonrpc::error_code::server_error );
   BOOST_REQUIRE( res.error->message == "a server error has occurred" );
   BOOST_REQUIRE( res.error->data.value() == "cannot divide by zero" );

   BOOST_TEST_MESSAGE( "sending request that has a malformed json request" );

   write_http( "{ \"jsonrpc\": \"2.0\", \"id\": 189, \"method\": \"add\", \"params\": { \"a\": 1, \"b\": 2 } ]" );

   res = read_response();

   BOOST_TEST_MESSAGE( "-> verifying result" );
   BOOST_REQUIRE_EQUAL( res.jsonrpc, "2.0" );
   BOOST_REQUIRE( std::get< std::nullptr_t >( res.id ) == nullptr );
   BOOST_REQUIRE( !res.result.has_value() );
   BOOST_REQUIRE( res.error->code == jsonrpc::error_code::parse_error );
   BOOST_REQUIRE( res.error->message == "unable to parse request" );
   BOOST_REQUIRE( res.error->data.has_value() );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_SUITE_END()
