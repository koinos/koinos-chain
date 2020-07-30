#include <boost/test/unit_test.hpp>

#include <koinos/crypto/elliptic.hpp>

#include <koinos/tests/crypto_fixture.hpp>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <vector>

BOOST_FIXTURE_TEST_SUITE( crypto_tests, crypto_fixture )

BOOST_AUTO_TEST_CASE( ripemd160_test )
{
   test( CRYPTO_RIPEMD160_ID,  TEST1, "8eb208f7e05d987a9b044a8e98c6b087f15a0bfc" );
   test( CRYPTO_RIPEMD160_ID,  TEST2, "9c1185a5c5e9fc54612808977ee8f548b2258d31" );
   test( CRYPTO_RIPEMD160_ID,  TEST3, "12a053384a9c0c88e405a06c27dcf49ada62eb2b" );
   test( CRYPTO_RIPEMD160_ID,  TEST4, "6f3fa39b6b503c384f919a49a7aa5c2c08bdfb45" );
   test( CRYPTO_RIPEMD160_ID,  TEST5, "52783243c1697bdbe16d37f97f68f08325dc1528" );
   test_big( CRYPTO_RIPEMD160_ID,  "29b6df855772aa9a95442bf83b282b495f9f6541" );
}

BOOST_AUTO_TEST_CASE( sha1_test )
{
   test( CRYPTO_SHA1_ID,  TEST1, "a9993e364706816aba3e25717850c26c9cd0d89d" );
   test( CRYPTO_SHA1_ID,  TEST2, "da39a3ee5e6b4b0d3255bfef95601890afd80709" );
   test( CRYPTO_SHA1_ID,  TEST3, "84983e441c3bd26ebaae4aa1f95129e5e54670f1" );
   test( CRYPTO_SHA1_ID,  TEST4, "a49b2446a02c645bf419f995b67091253a04a259" );
   test( CRYPTO_SHA1_ID,  TEST5, "34aa973cd4c4daa4f61eeb2bdbad27316534016f" );
   test_big( CRYPTO_SHA1_ID,  "7789f0c9ef7bfc40d93311143dfbe69e2017f592" );
}

BOOST_AUTO_TEST_CASE( sha256_test )
{
   test( CRYPTO_SHA2_256_ID,  TEST1, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" );
   test( CRYPTO_SHA2_256_ID,  TEST2, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" );
   test( CRYPTO_SHA2_256_ID,  TEST3, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" );
   test( CRYPTO_SHA2_256_ID,  TEST4, "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1" );
   test( CRYPTO_SHA2_256_ID,  TEST5, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0" );
   test_big( CRYPTO_SHA2_256_ID,  "50e72a0e26442fe2552dc3938ac58658228c0cbfb1d2ca872ae435266fcd055e" );
}

BOOST_AUTO_TEST_CASE( sha512_test )
{
   test( CRYPTO_SHA2_512_ID,  TEST1, "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
                           "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f" );
   test( CRYPTO_SHA2_512_ID,  TEST2, "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
                           "47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e" );
   test( CRYPTO_SHA2_512_ID,  TEST3, "204a8fc6dda82f0a0ced7beb8e08a41657c16ef468b228a8279be331a703c335"
                           "96fd15c13b1b07f9aa1d3bea57789ca031ad85c7a71dd70354ec631238ca3445" );
   test( CRYPTO_SHA2_512_ID,  TEST4, "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
                           "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909" );
   test( CRYPTO_SHA2_512_ID,  TEST5, "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973eb"
                           "de0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b" );
   test_big( CRYPTO_SHA2_512_ID,  "b47c933421ea2db149ad6e10fce6c7f93d0752380180ffd7f4629a712134831d"
                        "77be6091b819ed352c2967a2e2d4fa5050723c9630691f1a05a7281dbe6c1086" );
}

BOOST_AUTO_TEST_CASE( ecc )
{
   private_key nullkey;
   std::string pass = "foobar";

   for( uint32_t i = 0; i < 100; ++ i )
   {
      multihash h = hash_str( CRYPTO_SHA2_256_ID, pass.c_str(), pass.size() );
      private_key priv = private_key::regenerate( h );
      BOOST_CHECK( nullkey != priv );
      public_key pub = priv.get_public_key();

      pass += "1";
      multihash h2 = hash_str( CRYPTO_SHA2_256_ID, pass.c_str(), pass.size() );
      public_key  pub1  = pub.add( h2 );
      private_key priv1 = private_key::generate_from_seed(h, h2);

      std::string b58 = pub1.to_base58();
      public_key pub2 = public_key::from_base58(b58);
      BOOST_CHECK( pub1 == pub2 );

      auto sig = priv.sign_compact( h );
      auto recover = public_key::recover( sig, h );
      BOOST_CHECK( recover == pub );
   }
}

BOOST_AUTO_TEST_CASE( private_wif )
{
   std::string secret = "foobar";
   std::string wif = "5KJTiKfLEzvFuowRMJqDZnSExxxwspVni1G4RcggoPtDqP5XgM1";
   private_key key1 = private_key::regenerate( hash_str( CRYPTO_SHA2_256_ID, secret.c_str(), secret.size() ) );
   BOOST_CHECK_EQUAL( key1.to_wif(), wif );

   private_key key2 = private_key::from_wif( wif );
   BOOST_CHECK( key1 == key2 );

   // Encoding:
   // Prefix Secret                                                           Checksum
   // 80     C3AB8FF13720E8AD9047DD39466B3C8974E592C2FA383D4A3960714CAEF0C4F2 C957BEB4

   // Wrong checksum, change last octal (4->3)
   wif = "5KJTiKfLEzvFuowRMJqDZnSExxxwspVni1G4RcggoPtDqP5XgLz";
   BOOST_REQUIRE_THROW( private_key::from_wif( wif ), koinos::crypto::key_serialization_error );

   // Wrong seed, change first octal of secret (C->D)
   wif = "5KRWQqW5riLTcB39nLw6K7iv2HWBMYvbP7Ch4kUgRd8kEvLH5jH";
   BOOST_REQUIRE_THROW( private_key::from_wif( wif ), koinos::crypto::key_serialization_error );

   // Wrong prefix, change first octal of prefix (8->7)
   wif = "4nCYtcUpcC6dkge8r2uEJeqrK97TUZ1n7n8LXDgLtun1wRyxU2P";
   BOOST_REQUIRE_THROW( private_key::from_wif( wif ), koinos::crypto::key_serialization_error );
}

BOOST_AUTO_TEST_CASE( public_address )
{
   std::string private_wif = "5J1F7GHadZG3sCCKHCwg8Jvys9xUbFsjLnGec4H125Ny1V9nR6V";
   auto priv_key = private_key::from_wif( private_wif );
   auto pub_key = priv_key.get_public_key();
   auto address = pub_key.to_address();

   BOOST_REQUIRE_EQUAL( address, "1PMycacnJaSqwwJqjawXBErnLsZ7RkXUAs" );
}

BOOST_AUTO_TEST_CASE( zerohash )
{
   multihash mh;
   mh = zero_hash( CRYPTO_SHA2_256_ID );
   BOOST_CHECK( mh.id == CRYPTO_SHA2_256_ID );
   BOOST_CHECK( mh.digest.size() == 256/8 );

   mh = zero_hash( CRYPTO_RIPEMD160_ID );
   BOOST_CHECK( mh.id == CRYPTO_RIPEMD160_ID );
   BOOST_CHECK( mh.digest.size() == 160/8 );
}

BOOST_AUTO_TEST_CASE( emptyhash )
{
   multihash mh = empty_hash( CRYPTO_SHA2_256_ID );
   BOOST_CHECK_EQUAL( "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", hex_string( mh.digest ) );
}

BOOST_AUTO_TEST_CASE( merkle )
{
   multihash mh;
   std::vector< std::string > values {
      "the", "quick", "brown", "fox", "jumps", "over", "a", "lazy", "dog" };
   std::vector< std::string > wh_hex {
      "b9776d7ddf459c9ad5b0e1d6ac61e27befb5e99fd62446677600d7cacef544d0",
      "22c72aa82ce77c82e2ca65a711c79eaa4b51c57f85f91489ceeacc7b385943ba",
      "5eb67f9f8409b9c3f739735633cbdf92121393d0e13bd0f464b1b2a6a15ad2dc",
      "776cb326ab0cd5f0a974c1b9606044d8485201f2db19cf8e3749bdee5f36e200",
      "ef30940a2d1b943c8007b8a15e45935dc01902b7c0534dc7e27fda30a9b81aef",
      "5fb6a47e368e12e5d8b19280796e6a3d146fe391ed2e967d5f95c55bfb0f9c2f",
      "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb",
      "81fd67d02f679b818a4df6a50139958aa857eddc4d8f3561630dfb905e6d3c24",
      "cd6357efdd966de8c0cb2f876cc89ec74ce35f0968e11743987084bd42fb8944"
      };
   auto h = [&]( const multihash& ha, const multihash& hb ) -> multihash
   {
      std::vector<char> temp;
      std::copy( ha.digest.begin(), ha.digest.end(), std::back_inserter( temp ) );
      std::copy( hb.digest.begin(), hb.digest.end(), std::back_inserter( temp ) );
      std::cout << "temp: " << hex_string( temp ) << std::endl;
      multihash result = hash_str( CRYPTO_SHA2_256_ID, temp.data(), temp.size() );
      return result;
   };

   // Hash of each word
   std::vector< multihash > wh;
   for( size_t i = 0; i < values.size(); i++ )
   {
      wh.push_back( hash_str( CRYPTO_SHA2_256_ID, values[i].c_str(), values[i].size() ) );
      BOOST_CHECK_EQUAL( wh_hex[i], hex_string( wh[i].digest ) );
   }

   const std::string n01          = "0020397085ab4494829e691c49353a04d3201fda20c6a8a6866cf0f84bb8ce47";
   const std::string n23          = "78d4e37706320c82b2dd092eeb04b1f271523f86f910bf680ff9afcb2f8a33e1";
   const std::string n0123        = "e07aa684d91ffcbb89952f5e99b6181f7ee7bd88bd97be1345fc508f1062c050";
   const std::string n45          = "4185f41c5d7980ae7d14ce248f50e2854826c383671cf1ee3825ea957315c627";
   const std::string n67          = "b2a6704395c45ad8c99247103b580f7e7a37f06c3d38075ce4b02bc34c6a6754";
   const std::string n4567        = "2f24a249901ee8392ba0bb3b90c8efd6e2fee6530f45769199ef82d0b091d8ba";
   const std::string n01234567    = "913b7dce068efc8db6fab0173481f137ce91352b341855a1719aaff926169987";
   const std::string n8           = "cd6357efdd966de8c0cb2f876cc89ec74ce35f0968e11743987084bd42fb8944";
   const std::string n012345678   = "e24e552e0b6cf8835af179a14a766fb58c23e4ee1f7c6317d57ce39cc578cfac";

   multihash h01             = h( wh[0], wh[1] );
   multihash h23             = h( wh[2], wh[3] );
   multihash h0123           = h( h01, h23 );
   multihash h45             = h( wh[4], wh[5] );
   multihash h67             = h( wh[6], wh[7] );
   multihash h4567           = h( h45, h67 );
   multihash h01234567       = h( h0123, h4567 );
   multihash h8              = wh[8];
   multihash h012345678      = h( h01234567, h8 );

   BOOST_CHECK_EQUAL( n01       , hex_string(        h01.digest ) );
   BOOST_CHECK_EQUAL( n23       , hex_string(        h23.digest ) );
   BOOST_CHECK_EQUAL( n0123     , hex_string(      h0123.digest ) );
   BOOST_CHECK_EQUAL( n45       , hex_string(        h45.digest ) );
   BOOST_CHECK_EQUAL( n67       , hex_string(        h67.digest ) );
   BOOST_CHECK_EQUAL( n4567     , hex_string(      h4567.digest ) );
   BOOST_CHECK_EQUAL( n01234567 , hex_string(  h01234567.digest ) );
   BOOST_CHECK_EQUAL( n012345678, hex_string( h012345678.digest ) );

   multihash merkle_root;

   std::vector< variable_blob > blob_values;
   for( size_t i = 0; i < values.size(); i++ )
   {
      blob_values.emplace_back( values[i].begin(), values[i].end() );
   }

   merkle_root = merkle_hash( CRYPTO_SHA2_256_ID, blob_values );
   BOOST_CHECK_EQUAL( n012345678, hex_string( merkle_root.digest ) );
}

BOOST_AUTO_TEST_SUITE_END()
