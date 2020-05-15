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

   void test( uint64_t code, const char* to_hash, const std::string& expected )
   {
      multihash_type mh1 = hash( code, to_hash, strlen( to_hash ) );
      json j;
      koinos::pack::to_json( j, mh1 );
      BOOST_CHECK_EQUAL( expected, j.dump() );
      multihash_type mh2;
      j = expected;
      koinos::pack::from_json( j, mh2 );
      BOOST_CHECK( mh1 == mh2 );
   }

   void test( uint64_t code, const std::string& to_hash, const std::string& expected )
   {
      test( code, to_hash.c_str(), expected );
   }

   void test_big( uint64_t code, const std::string& expected )
   {
      encoder enc( code );
      for (char c : TEST6) { enc.put(c); }
      for (int i = 0; i < 16777215; i++) {
         enc.write( TEST6.c_str(), TEST6.size() );
      }
      multihash_type mh;
      enc.get_result( mh );
      json j;
      koinos::pack::to_json( j, mh );
      BOOST_CHECK_EQUAL( expected, j.dump() );

      enc.reset();
      enc.write( TEST1.c_str(), TEST1.size() );
      enc.get_result( mh );
      //BOOST_CHECK( hash >= hash( code, TEST1.c_str(), TEST1.size() ) );
      koinos::pack::to_json( j, mh );
      test( code, TEST1, j.dump() );

      //hash = hash ^ hash;
      //hash.data()[hash.data_size() - 1] = 1;
      //for (int i = hash.data_size() * 8 - 1; i > 0; i--) {
      //   H other = hash << i;
      //   BOOST_CHECK( other != hash );
      //   BOOST_CHECK( other > hash );
      //   BOOST_CHECK( hash < other );
      //}

      multihash_type mh2;
      j = expected;
      koinos::pack::from_json( j, mh2 );
      koinos::pack::to_json( j, mh2 );
      koinos::pack::from_json( j, mh2 );
      BOOST_CHECK( mh == mh2 );
   }
};
