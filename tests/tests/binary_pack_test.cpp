
#include <boost/test/unit_test.hpp>

#include "../test_fixtures/pack_fixture.hpp"

#include <koinos/pack/rt/binary.hpp>
#include <koinos/pack/rt/varint.hpp>

#define REQUIRE_DEEP_EQUAL( s, v )                         \
do{                                                        \
   BOOST_REQUIRE_EQUAL( s.tellp() - s.tellg(), v.size() ); \
   std::stringstream copy( s.str() );                      \
   for( size_t i = 0; i < v.size(); i++ )                  \
   {                                                       \
      uint8_t c;                                           \
      copy.get( (char&)c );                                \
      BOOST_CHECK_EQUAL( v[i], c );                        \
   }                                                       \
} while(0);

BOOST_FIXTURE_TEST_SUITE( binary_pack_tests, pack_fixture )

using namespace koinos::pack;

BOOST_AUTO_TEST_CASE( basic_test )
{
   std::stringstream ss;
   to_binary( ss, int64_t(-256) );

   vector< uint8_t > expected = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };
   REQUIRE_DEEP_EQUAL( ss, expected );

   int64_t res = 0;
   from_binary( ss, res );
   BOOST_REQUIRE_EQUAL( res, -256 );
}

BOOST_AUTO_TEST_CASE( uint128_test )
{
   uint128_t to_bin = 1;
   to_bin <<= 65;
   to_bin -= 1; // 2^65 - 1

   std::stringstream ss;
   to_binary( ss, to_bin );

   vector< uint8_t > expected = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
   };
   REQUIRE_DEEP_EQUAL( ss, expected );

   uint128_t from_bin;
   from_binary( ss, from_bin );
   BOOST_REQUIRE_EQUAL( from_bin, to_bin );
}

BOOST_AUTO_TEST_CASE( uint160_test )
{
   uint160_t to_bin = 1;
   to_bin <<= 129;
   to_bin -= 1; // 2^129 - 1

   std::stringstream ss;
   to_binary( ss, to_bin );

   vector< uint8_t > expected = {
      0x00, 0x00, 0x00, 0x01,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
   };
   REQUIRE_DEEP_EQUAL( ss, expected );

   uint160_t from_bin;
   from_binary( ss, from_bin );
   BOOST_REQUIRE_EQUAL( from_bin, to_bin );
}

BOOST_AUTO_TEST_CASE( uint256_test )
{
   uint256_t to_bin = 1;
   to_bin <<= 129;
   to_bin -= 1; // 2^129 - 1

   std::stringstream ss;
   to_binary( ss, to_bin );

   vector< uint8_t > expected = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
   };
   REQUIRE_DEEP_EQUAL( ss, expected );

   uint256_t from_bin;
   from_binary( ss, from_bin );
   BOOST_REQUIRE_EQUAL( from_bin, to_bin );
}

BOOST_AUTO_TEST_CASE( unsigned_varint_test )
{
   std::stringstream ss;
   unsigned_int to_bin = 256;
   to_binary( ss, to_bin );

   vector< uint8_t > expected = { 0x82, 0x00 };
   REQUIRE_DEEP_EQUAL( ss, expected );

   unsigned_int from_bin = 0;
   from_binary( ss, from_bin );
   BOOST_REQUIRE_EQUAL( from_bin.value, 256 );
}

BOOST_AUTO_TEST_CASE( signed_varint_test )
{
   std::stringstream ss;
   signed_int to_bin = -254;
   to_binary( ss, to_bin );

   vector< uint8_t > expected = { 0x83, 0x7B };
   REQUIRE_DEEP_EQUAL( ss, expected );

   signed_int from_bin;
   from_binary( ss, from_bin );
   BOOST_REQUIRE_EQUAL( from_bin.value, -254 );

   ss = std::stringstream();
   to_bin = 256;
   to_binary( ss, to_bin );

   expected = { 0x84, 0x00 };
   REQUIRE_DEEP_EQUAL( ss, expected );

   from_binary( ss, from_bin );
   BOOST_REQUIRE_EQUAL( from_bin.value, 256 );
}

BOOST_AUTO_TEST_CASE( signed_varint_mapping )
{
   std::stringstream ss;
   signed_int to_bin_pos = 0;
   signed_int to_bin_neg = 0;

   to_binary( ss, to_bin_pos );

   unsigned_int from_bin;
   from_binary( ss, from_bin );

   BOOST_REQUIRE_EQUAL( from_bin.value, 0 );

   for( int64_t i = 1; i <= 256; ++i )
   {
      to_bin_pos.value = i;
      to_bin_neg.value = -i;

      to_binary( ss, to_bin_pos );
      to_binary( ss, to_bin_neg );

      from_binary( ss, from_bin );
      BOOST_REQUIRE_EQUAL( from_bin.value, 2 * i );

      from_binary( ss, from_bin );
      BOOST_REQUIRE_EQUAL( from_bin.value, (2 * i) - 1 );
   }
}

BOOST_AUTO_TEST_CASE( vector_test )
{
   std::stringstream ss;
   vector< int16_t > to_bin = { 4, 8, 15, 16, 23, 42 };

   to_binary( ss, to_bin );

   vector< uint8_t > expected = {
      0x06,       // size
      0x00, 0x04, // 4
      0x00, 0x08, // 8
      0x00, 0x0F, // 15
      0x00, 0x10, // 16
      0x00, 0x17, // 23
      0x00, 0x2A  // 42
   };

   REQUIRE_DEEP_EQUAL( ss, expected );

   vector< int16_t > from_bin;
   from_binary( ss, from_bin );

   BOOST_REQUIRE_EQUAL( to_bin.size(), from_bin.size() );
   for( size_t i = 0; i < to_bin.size(); ++i )
   {
      BOOST_REQUIRE_EQUAL( to_bin[i], from_bin[i] );
   }

   unsigned_int size = KOINOS_PACK_MAX_ARRAY_ALLOC_SIZE / sizeof( int16_t ) + 1;
   to_binary( ss, size );
   for( size_t i = 0; i < size.value; ++i )
   {
      // 10MB of 0s
      to_binary( ss, int64_t(0) );
   }

   try
   {
      from_binary( ss, from_bin );
      BOOST_FAIL( "allcoation_violation not thrown" );
   }
   catch( allocation_violation& ) {}
}

BOOST_AUTO_TEST_CASE( array_test )
{
   std::stringstream ss;
   std::array< int16_t, 6 > to_bin = { 4, 8, 15, 16, 23, 42 };

   to_binary( ss, to_bin );

   vector< uint8_t > expected = {
      0x00, 0x04, // 4
      0x00, 0x08, // 8
      0x00, 0x0F, // 15
      0x00, 0x10, // 16
      0x00, 0x17, // 23
      0x00, 0x2A  // 42
   };

   REQUIRE_DEEP_EQUAL( ss, expected );

   std::array< int16_t, 6 > from_bin;
   from_binary( ss, from_bin );

   for( size_t i = 0; i < to_bin.size(); ++i )
   {
      BOOST_REQUIRE_EQUAL( to_bin[i], from_bin[i] );
   }

   // Array alloction failure is checked statically, so it does not require a runtime test

   ss = std::stringstream();
   to_binary( ss, unsigned_int( 100 ) ); // Purposeful buffer overflow
   std::array< int16_t, 100 > large_array;
   BOOST_REQUIRE_THROW( from_binary( ss, large_array ), stream_error );
}

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

BOOST_AUTO_TEST_CASE( variant_test )
{
   typedef std::variant< int16_t, int32_t > test_variant;
   test_variant to_bin = int16_t( 10 );

   std::stringstream ss;
   to_binary( ss, to_bin );

   vector< uint8_t > expected = {
      0x00,       // index
      0x00, 0x0A  // value
   };

   REQUIRE_DEEP_EQUAL( ss, expected );

   test_variant from_bin;
   from_binary( ss, from_bin );

   std::visit( overloaded {
      []( int16_t v ){ BOOST_REQUIRE_EQUAL( v, 10 ); },
      []( int32_t v ){ BOOST_FAIL( "variant contains unexpected type" ); },
      []( auto& v ){ BOOST_FAIL( "variant contains unexpected type" ); }
   }, from_bin );

   ss = std::stringstream();
   to_bin = int32_t( 20 );
   to_binary( ss, to_bin );

   expected = {
      0x01,                   // index
      0x00, 0x00, 0x00, 0x14  // value
   };

   REQUIRE_DEEP_EQUAL( ss, expected );

   from_binary( ss, from_bin );

   std::visit( overloaded {
      []( int16_t v ){ BOOST_FAIL( "variant contains unexpected type" ); },
      []( int32_t v ){ BOOST_REQUIRE_EQUAL( v, 20 ); },
      []( auto& v ){ BOOST_FAIL( "variant contains unexpected type" ); }
   }, from_bin );

   ss = std::stringstream();
   unsigned_int tag = 2; //invalid variant tag
   to_binary( ss, tag );
   to_binary( ss, int16_t( 10 ) );

   try
   {
      from_binary( ss, from_bin );
      BOOST_FAIL( "Unexpected tag did not throw exception" );
   }
   catch( parse_error& ) {}
}

BOOST_AUTO_TEST_CASE( optional_test )
{
   typedef std::optional< int16_t > test_optional;
   test_optional to_bin;

   std::stringstream ss;
   to_binary( ss, to_bin );

   vector< uint8_t > expected = { 0x00 };
   REQUIRE_DEEP_EQUAL( ss, expected );

   test_optional from_bin;
   from_binary( ss, from_bin );
   BOOST_REQUIRE( !from_bin.has_value() );

   ss = std::stringstream();
   to_bin = 10;
   to_binary( ss, to_bin );

   expected = { 0x01, 0x00, 0x0A };
   REQUIRE_DEEP_EQUAL( ss, expected );

   from_binary( ss, from_bin );
   BOOST_REQUIRE( from_bin.has_value() );
   BOOST_REQUIRE_EQUAL( *from_bin, *to_bin );
}

BOOST_AUTO_TEST_CASE( vl_blob_test )
{
   vl_blob to_bin;
   to_bin.data = { 0x04, 0x08, 0x0F, 0x10, 0x17, 0x2A };

   std::stringstream ss;
   to_binary( ss, to_bin );

   vector< uint8_t > expected = { 0x06, 0x04, 0x08, 0x0F, 0x10, 0x17, 0x2A };
   REQUIRE_DEEP_EQUAL( ss, expected );

   vl_blob from_bin;
   from_binary( ss, from_bin );
   BOOST_REQUIRE_EQUAL( to_bin.data.size(), from_bin.data.size() );
   for( size_t i = 0; i < to_bin.data.size(); ++i )
   {
      BOOST_REQUIRE_EQUAL( to_bin.data[i], from_bin.data[i] );
   }
}

BOOST_AUTO_TEST_CASE( fl_blob_test )
{
   fl_blob< 6 > to_bin;
   to_bin.data = { 0x04, 0x08, 0x0F, 0x10, 0x17, 0x2A };

   std::stringstream ss;
   to_binary( ss, to_bin );

   vector< uint8_t > expected = { 0x04, 0x08, 0x0F, 0x10, 0x17, 0x2A };
   REQUIRE_DEEP_EQUAL( ss, expected );

   fl_blob< 6 > from_bin;
   from_binary( ss, from_bin );
   for( size_t i = 0; i < to_bin.data.size(); ++i )
   {
      BOOST_REQUIRE_EQUAL( to_bin.data[i], from_bin.data[i] );
   }
}

BOOST_AUTO_TEST_CASE( multihash_type_test )
{
   multihash_type to_bin;
   to_bin.hash_id = 1;
   to_bin.digest.data = { 0x04, 0x08, 0x0F, 0x10, 0x17, 0x2A };

   std::stringstream ss;
   to_binary( ss, to_bin );

   vector< uint8_t > expected = { 0x01, 0x06, 0x04, 0x08, 0x0F, 0x10, 0x17, 0x2A };
   REQUIRE_DEEP_EQUAL( ss, expected );

   multihash_type from_bin;
   from_binary( ss, from_bin );
   BOOST_REQUIRE_EQUAL( to_bin.digest.data.size(), from_bin.digest.data.size() );
   for( size_t i = 0; i < to_bin.digest.data.size(); ++i )
   {
      BOOST_REQUIRE_EQUAL( to_bin.digest.data[i], from_bin.digest.data[i] );
   }
}

BOOST_AUTO_TEST_CASE( multihash_vector_test )
{
   multihash_vector to_bin;
   to_bin.hash_id = 1;
   vl_blob digest_a;
   digest_a.data = { 0x04, 0x08, 0x0F, 0x10, 0x17, 0x2A };
   vl_blob digest_b;
   digest_b.data = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
   to_bin.digests.push_back( digest_a );
   to_bin.digests.push_back( digest_b );

   std::stringstream ss;
   to_binary( ss, to_bin );

   vector< uint8_t > expected = {
      0x01, // hash_id
      0x06, // hash length
      0x02, // num hashes
      0x04, 0x08, 0x0F, 0x10, 0x17, 0x2A, // digest_a
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06  // digest_b
   };
   REQUIRE_DEEP_EQUAL( ss, expected );

   multihash_vector from_bin;
   from_binary( ss, from_bin );
   BOOST_REQUIRE_EQUAL( to_bin.hash_id, from_bin.hash_id );
   BOOST_REQUIRE_EQUAL( to_bin.digests.size(), from_bin.digests.size() );
   BOOST_REQUIRE_EQUAL( to_bin.digests[0].data.size(), from_bin.digests[0].data.size() );
   BOOST_REQUIRE_EQUAL( to_bin.digests[1].data.size(), from_bin.digests[1].data.size() );
   for( size_t i = 0; i < to_bin.digests[0].data.size(); ++i )
   {
      BOOST_REQUIRE_EQUAL( to_bin.digests[0].data[i], from_bin.digests[0].data[i] );
      BOOST_REQUIRE_EQUAL( to_bin.digests[1].data[i], from_bin.digests[1].data[i] );
   }
}

BOOST_AUTO_TEST_CASE( reflect_test )
{
   test_object to_bin;
   to_bin.id.data = { 0, 4, 8, 15, 16, 23, 42, 0 };
   to_bin.key.hash_id = 1;
   to_bin.key.digest.data = { 'f', 'o', 'o', 'b', 'a', 'r' };
   to_bin.vals = { 108 };

   std::stringstream ss;
   to_binary( ss, to_bin );

   vector< uint8_t > expected = {
      0x00, 0x04, 0x08, 0x0F, 0x10, 0x17, 0x2A, 0x00, // id
      0x01, 0x06, 0x66, 0x6F, 0x6F, 0x62, 0x61, 0x72, // key
      0x01, 0x00, 0x00, 0x00, 0x6C                    // vals
   };
   REQUIRE_DEEP_EQUAL( ss, expected );

   test_object from_bin;
   from_binary( ss, from_bin );
   BOOST_REQUIRE( memcmp( from_bin.id.data.data(), to_bin.id.data.data(), to_bin.id.data.size() ) == 0 );
   BOOST_REQUIRE_EQUAL( from_bin.key.digest.data.size(), to_bin.key.digest.data.size() );
   BOOST_REQUIRE( memcmp( from_bin.key.digest.data.data(), to_bin.key.digest.data.data(), to_bin.key.digest.data.size() ) == 0 );
   BOOST_REQUIRE_EQUAL( from_bin.vals.size(), to_bin.vals.size() );
   BOOST_REQUIRE( memcmp( (char*)from_bin.vals.data(), (char*)to_bin.vals.data(), sizeof(uint32_t) * to_bin.vals.size() ) == 0 );
}

/*
BOOST_AUTO_TEST_CASE( depth_test )
{
   // The compiler is not happy about the template deduction I am about to make it do,
   // but it will complete it...
   vector< vector< vector< vector< vector< vector< vector< vector< vector< vector< vector<
      vector< vector< vector< vector< vector< vector< vector< vector< vector< vector< uint8_t >
   > > > > > > > > > > > > > > > > > > > > from_bin;

   std::stringstream ss;
   for( size_t i = 0; i < 20; ++i )
      ss << uint8_t( 1 );

   ss << uint8_t( 0 );

   try
   {
      from_binary( ss, from_bin );
      BOOST_FAIL( "depth_violation not raised" );
   }
   catch( depth_violation& ) {}
}
*/

BOOST_AUTO_TEST_SUITE_END()
