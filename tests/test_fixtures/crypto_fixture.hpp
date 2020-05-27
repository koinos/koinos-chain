#pragma once
#include <koinos/crypto/multihash.hpp>
#include <koinos/pack/rt/json.hpp>

#include <iostream>

using namespace koinos::crypto;
using nlohmann::json;

// SHA test vectors taken from http://www.di-mgt.com.au/sha_testvectors.html
static const std::string TEST1("abc");
static const std::string TEST2("");
static const std::string TEST3("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
static const std::string TEST4("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu");
static char TEST5[1000001];
static const std::string TEST6("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno");

static void init_5()
{
   memset( TEST5, 'a', sizeof(TEST5) - 1 );
   TEST5[1000000] = 0;
}

struct crypto_fixture
{
   crypto_fixture()
   {
      init_5();
   }

   template< typename Blob >
   std::string hex_string( const Blob& b )
   {
      static const char hex[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

      std::stringstream ss;

      for( auto c : b.data )
         ss << hex[(c & 0xF0) >> 4] << hex[c & 0x0F];

      return ss.str();
   }

   void test( uint64_t code, const std::string& to_hash, const std::string& expected )
   {
      multihash_type mh1 = hash( code, to_hash.c_str(), to_hash.size() );
      BOOST_CHECK_EQUAL( expected, hex_string( mh1.digest ) );
      BOOST_CHECK_EQUAL( code, multihash::get_id(mh1) );
      BOOST_CHECK_EQUAL( get_standard_size( code ), multihash::get_size(mh1) );

      // TODO: This check is not currently working, but a sample test case has been added to json_pack_tests

      //json j;
      //multihash_type mh2;
      //koinos::pack::to_json( j, mh1 );
      //koinos::pack::from_json( j, mh2 );
      //BOOST_CHECK( mh1 == mh2 );
   }

   void test_big( uint64_t code, const std::string& expected )
   {
      encoder enc( code );
      for (char c : TEST6) { enc.put(c); }
      for (int i = 0; i < 16777215; i++) {
         enc.write( TEST6.c_str(), TEST6.size() );
      }
      multihash_type mh1;
      enc.get_result( mh1 );
      BOOST_CHECK_EQUAL( expected, hex_string( mh1.digest ) );
      BOOST_CHECK_EQUAL( code, multihash::get_id(mh1) );
      BOOST_CHECK_EQUAL( get_standard_size( code ), multihash::get_size(mh1) );

      //json j;
      //multihash_type mh2;
      //koinos::pack::to_json( j, mh1 );
      //koinos::pack::from_json( j, mh2 );
      //BOOST_CHECK( mh1 == mh2 );
   }
};
