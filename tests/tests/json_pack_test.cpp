
#include <boost/test/unit_test.hpp>

#include "../test_fixtures/pack_fixture.hpp"

#include <koinos/pack/rt/json.hpp>

struct json_pack_fixture {};

BOOST_FIXTURE_TEST_SUITE( json_pack_tests, pack_fixture )

using namespace koinos::pack;

BOOST_AUTO_TEST_CASE( basic_test )
{
   json j;
   to_json( j, int64_t(-256) );

   std::string expected = "-256";
   BOOST_REQUIRE_EQUAL( expected, j.dump() );

   int64_t res = 0;
   from_json( j, res );
   BOOST_REQUIRE_EQUAL( res, -256 );
}

BOOST_AUTO_TEST_CASE( integer_bounds )
{
   json j;
   j = json::parse( "4294967296" ); // 2^32

   int32_t from_j;
   BOOST_REQUIRE_THROW( from_json( j, from_j ), json_int_out_of_bounds );
}

BOOST_AUTO_TEST_CASE( json_integer_bounds )
{
   json j;
   const int64_t one = 1;

   to_json( j, (one<<53) - 1 );
   std::string expected = "9007199254740991";
   BOOST_REQUIRE_EQUAL( expected, j.dump() );

   to_json( j, one<<53 );
   expected = "\"9007199254740992\"";
   BOOST_REQUIRE_EQUAL( expected, j.dump() );

   to_json( j, -((one<<53) - 1) );
   expected = "-9007199254740991";
   BOOST_REQUIRE_EQUAL( expected, j.dump() );

   to_json( j, -(one<<53) );
   expected = "\"-9007199254740992\"";
   BOOST_REQUIRE_EQUAL( expected, j.dump() );
}

BOOST_AUTO_TEST_CASE( uint128_test )
{
   uint128_t to_j = 1;
   to_j <<= 65;
   to_j -= 1; // 2^65 - 1

   json j;
   to_json( j, to_j );

   std::string expected = "\"36893488147419103231\"";
   BOOST_REQUIRE_EQUAL( expected, j.dump() );

   uint128_t from_j;
   from_json( j, from_j );
   BOOST_REQUIRE_EQUAL( from_j, to_j );

   to_j = 10;
   to_json( j, to_j );

   expected = "10";
   BOOST_REQUIRE_EQUAL( j.dump(), expected );
}

BOOST_AUTO_TEST_CASE( uint160_test )
{
   uint160_t to_j = 1;
   to_j <<= 129;
   to_j -= 1; // 2^129 - 1

   json j;
   to_json( j, to_j );

   std::string expected = "\"680564733841876926926749214863536422911\"";
   BOOST_REQUIRE_EQUAL( j.dump(), expected );

   uint160_t from_j;
   from_json( j, from_j );
   BOOST_REQUIRE_EQUAL( from_j, to_j );

   to_j = 10;
   to_json( j, to_j );

   expected = "10";
   BOOST_REQUIRE_EQUAL( j.dump(), expected );
}

BOOST_AUTO_TEST_CASE( uint256_test )
{
   uint256_t to_j = 1;
   to_j <<= 129;
   to_j -= 1; // 2^129 - 1

   json j;
   to_json( j, to_j );

   std::string expected = "\"680564733841876926926749214863536422911\"";
   BOOST_REQUIRE_EQUAL( j.dump(), expected );

   uint256_t from_j;
   from_json( j, from_j );
   BOOST_REQUIRE_EQUAL( from_j, to_j );

   to_j = 10;
   to_json( j, to_j );

   expected = "10";
   BOOST_REQUIRE_EQUAL( j.dump(), expected );
}

BOOST_AUTO_TEST_CASE( vector_test )
{
   json j;
   vector< int16_t > to_j = { 4, 8, 15, 16, 23, 42 };

   to_json( j, to_j );

   std::string expected = "[4,8,15,16,23,42]";
   BOOST_REQUIRE_EQUAL( j.dump(), expected );

   vector< int16_t > from_j;
   from_json( j, from_j );

   BOOST_REQUIRE_EQUAL( to_j.size(), from_j.size() );
   for( size_t i = 0; i < to_j.size(); ++i )
   {
      BOOST_REQUIRE_EQUAL( to_j[i], from_j[i] );
   }

   j = json::parse( "[\"foo\",\"bar\"]" );
   BOOST_REQUIRE_THROW( from_json( j, from_j ), std::exception );
}

BOOST_AUTO_TEST_CASE( array_test )
{
   json j;
   std::array< int16_t, 6 > to_j = { 4, 8, 15, 16, 23, 42 };

   to_json( j, to_j );

   std::string expected = "[4,8,15,16,23,42]";
   BOOST_REQUIRE_EQUAL( j.dump(), expected );

   std::array< int16_t, 6 > from_j;
   from_json( j, from_j );

   for( size_t i = 0; i < to_j.size(); ++i )
   {
      BOOST_REQUIRE_EQUAL( to_j[i], from_j[i] );
   }

   j = json::parse( "[4,8,15,16,23,42,108]") ;
   BOOST_REQUIRE_THROW( from_json( j, from_j ), json_type_mismatch );

   j = json::parse( "[\"foo\",\"bar\",\"a\",\"b\",\"c\",\"d\"]" );
   BOOST_REQUIRE_THROW( from_json( j, from_j ), std::exception );
}

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

BOOST_AUTO_TEST_CASE( variant_test )
{
   typedef std::variant< int16_t, int32_t > test_variant;
   test_variant to_j = int16_t( 10 );

   json j;
   to_json( j, to_j );

   std::string expected = "{\"type\":\"int16_t\",\"value\":10}";
   BOOST_REQUIRE_EQUAL( j.dump(), expected );

   test_variant from_j;
   from_json( j, from_j );

   std::visit( overloaded {
      []( int16_t v ){ BOOST_REQUIRE_EQUAL( v, 10 ); },
      []( int32_t v ){ BOOST_FAIL( "variant contains unexpected type" ); },
      []( auto& v ){ BOOST_FAIL( "variant contains unexpected type" ); }
   }, from_j );

   j["type"] = 0;
   from_json( j, from_j );

   std::visit( overloaded {
      []( int16_t v ){ BOOST_REQUIRE_EQUAL( v, 10 ); },
      []( int32_t v ){ BOOST_FAIL( "variant contains unexpected type" ); },
      []( auto& v ){ BOOST_FAIL( "variant contains unexpected type" ); }
   }, from_j );

   to_j = int32_t( 20 );
   to_json( j, to_j );

   expected = expected = "{\"type\":\"int32_t\",\"value\":20}";

   BOOST_REQUIRE_EQUAL( j.dump(), expected );

   from_json( j, from_j );

   std::visit( overloaded {
      []( int16_t v ){ BOOST_FAIL( "variant contains unexpected type" ); },
      []( int32_t v ){ BOOST_REQUIRE_EQUAL( v, 20 ); },
      []( auto& v ){ BOOST_FAIL( "variant contains unexpected type" ); }
   }, from_j );

   j["type"] = 1;
   from_json( j, from_j );

   std::visit( overloaded {
      []( int16_t v ){ BOOST_FAIL( "variant contains unexpected type" ); },
      []( int32_t v ){ BOOST_REQUIRE_EQUAL( v, 20 ); },
      []( auto& v ){ BOOST_FAIL( "variant contains unexpected type" ); }
   }, from_j );

   j["type"] = 2;
   BOOST_REQUIRE_THROW( from_json( j, from_j ), parse_error );

   j["type"] = "uint64_t";
   BOOST_REQUIRE_THROW( from_json( j, from_j ), json_type_mismatch );
}

BOOST_AUTO_TEST_CASE( optional_test )
{
   typedef std::optional< int16_t > test_optional;
   test_optional to_j;

   json j;
   to_json( j, to_j );

   BOOST_REQUIRE( j.is_null() );

   test_optional from_j;
   from_json( j, from_j );
   BOOST_REQUIRE( !from_j.has_value() );

   to_j = 10;
   to_json( j, to_j );

   std::string expected = "10";
   BOOST_REQUIRE_EQUAL( j.dump(), expected );

   from_json( j, from_j );
   BOOST_REQUIRE( from_j.has_value() );
   BOOST_REQUIRE_EQUAL( *from_j, *to_j );
}

BOOST_AUTO_TEST_CASE( vl_blob_test )
{
   vl_blob to_j;
   to_j.data = { 0x04, 0x08, 0x0F, 0x10, 0x17, 0x2A };

   json j;
   to_json( j, to_j );

   std::string expected = "\"z31SRtpx1\"";
   BOOST_REQUIRE_EQUAL( j.dump(), expected );

   vl_blob from_j;
   from_json( j, from_j );
   BOOST_REQUIRE_EQUAL( to_j.data.size(), from_j.data.size() );
   for( size_t i = 0; i < to_j.data.size(); ++i )
   {
      BOOST_REQUIRE_EQUAL( to_j.data[i], from_j.data[i] );
   }
}

BOOST_AUTO_TEST_CASE( fl_blob_test )
{
   fl_blob< 6 > to_j;
   to_j.data = { 0x04, 0x08, 0x0F, 0x10, 0x17, 0x2A };

   json j;
   to_json( j, to_j );

   std::string expected = "\"z31SRtpx1\"";
   BOOST_REQUIRE_EQUAL( j.dump(), expected );

   fl_blob< 6 > from_j;
   from_json( j, from_j );
   for( size_t i = 0; i < to_j.data.size(); ++i )
   {
      BOOST_CHECK_EQUAL( to_j.data[i], from_j.data[i] );
   }
}

void hex_to_vector(
   std::vector<char>& out,
   std::initializer_list<uint8_t> values)
{
   for( auto it=values.begin(); it!=values.end(); ++it )
   {
      out.push_back( uint8_t( *it ) );
   }
}

BOOST_AUTO_TEST_CASE( multihash_type_test )
{
   multihash_type to_j;
   to_j.hash_id = 1;
   to_j.digest.data = { 0x04, 0x08, 0x0F, 0x10, 0x17, 0x2A };

   json j;
   to_json( j, to_j );

   std::string expected = "{\"digest\":\"z31SRtpx1\",\"hash\":1}";
   BOOST_REQUIRE_EQUAL( j.dump(), expected );

   multihash_type from_j;
   from_json( j, from_j );
   BOOST_REQUIRE_EQUAL( to_j.digest.data.size(), from_j.digest.data.size() );
   for( size_t i = 0; i < to_j.digest.data.size(); ++i )
   {
      BOOST_REQUIRE_EQUAL( to_j.digest.data[i], from_j.digest.data[i] );
   }

   to_j.digest.data.clear();
   to_j.hash_id = 4640;
   hex_to_vector( to_j.digest.data, {
      0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA,
      0x41, 0x41, 0x40, 0xDE, 0x5D, 0xAE, 0x22, 0x23,
      0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C,
      0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD
   } );

   to_json( j, to_j );
   expected = "{\"digest\":\"zDYu3G8aGTMBW1WrTw76zxQJQU4DHLw9MLyy7peG4LKkY\",\"hash\":4640}";
   BOOST_REQUIRE_EQUAL( j.dump(), expected );
}

BOOST_AUTO_TEST_CASE( multihash_vector_test )
{
   multihash_vector to_j;
   to_j.hash_id = 1;
   vl_blob digest_a;
   digest_a.data = { 0x04, 0x08, 0x0F, 0x10, 0x17, 0x2A };
   vl_blob digest_b;
   digest_b.data = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
   to_j.digests.push_back( digest_a );
   to_j.digests.push_back( digest_b );

   json j;
   to_json( j, to_j );

   std::string expected = "{\"digests\":[\"z31SRtpx1\",\"zW7LcTy7\"],\"hash\":1}";
   BOOST_REQUIRE_EQUAL( j.dump(), expected );

   multihash_vector from_j;
   from_json( j, from_j );
   BOOST_REQUIRE_EQUAL( to_j.hash_id, from_j.hash_id );
   BOOST_REQUIRE_EQUAL( to_j.digests.size(), from_j.digests.size() );
   BOOST_REQUIRE_EQUAL( to_j.digests[0].data.size(), from_j.digests[0].data.size() );
   BOOST_REQUIRE_EQUAL( to_j.digests[1].data.size(), from_j.digests[1].data.size() );
   for( size_t i = 0; i < to_j.digests[0].data.size(); ++i )
   {
      BOOST_REQUIRE_EQUAL( to_j.digests[0].data[i], from_j.digests[0].data[i] );
      BOOST_REQUIRE_EQUAL( to_j.digests[1].data[i], from_j.digests[1].data[i] );
   }
}

BOOST_AUTO_TEST_CASE( reflect_test )
{
   test_object to_j;
   to_j.id.data = { 0, 4, 8, 15, 16, 23, 42, 0 };
   to_j.key.hash_id = 1;
   to_j.key.digest.data = { 'f', 'o', 'o', 'b', 'a', 'r' };
   to_j.vals = { 108 };

   json j;
   to_json( j, to_j );
   std::string expected = "{\"ext\":null,\"id\":\"z19rwEskdm1\",\"key\":{\"digest\":\"zt1Zv2yaZ\",\"hash\":1},\"vals\":[108]}";
   BOOST_REQUIRE_EQUAL( j.dump(), expected );

   test_object from_j;
   from_json( j, from_j );
   BOOST_REQUIRE( memcmp( from_j.id.data.data(), to_j.id.data.data(), to_j.id.data.size() ) == 0 );
   BOOST_REQUIRE_EQUAL( from_j.key.digest.data.size(), to_j.key.digest.data.size() );
   BOOST_REQUIRE( memcmp( from_j.key.digest.data.data(), to_j.key.digest.data.data(), to_j.key.digest.data.size() ) == 0 );
   BOOST_REQUIRE_EQUAL( from_j.vals.size(), to_j.vals.size() );
   BOOST_REQUIRE( memcmp( (char*)from_j.vals.data(), (char*)to_j.vals.data(), sizeof(uint32_t) * to_j.vals.size() ) == 0 );
}

/*
( depth_test )
{
   // The compiler is not happy about the template deduction I am about to make it do,
   // but it will complete it...
   vector< vector< vector< vector< vector< vector< vector< vector< vector< vector< vector<
      vector< vector< vector< vector< vector< vector< vector< vector< vector< vector< uint8_t >
   > > > > > > > > > > > > > > > > > > > > from_j;

   json j = json::parse( "[[[[[[[[[[[[[[[[[[[[[1]]]]]]]]]]]]]]]]]]]]]" );
   BOOST_REQUIRE_THROW( from_json( j, from_j ), depth_violation );
}
*/

BOOST_AUTO_TEST_SUITE_END()
