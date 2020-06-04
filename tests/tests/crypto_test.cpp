#include <boost/test/unit_test.hpp>

#include "../test_fixtures/crypto_fixture.hpp"

#include <koinos/crypto/elliptic.hpp>

#include <fc/crypto/base58.hpp>

#include <iostream>

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
      multihash_type h = hash_str( CRYPTO_SHA2_256_ID, pass.c_str(), pass.size() );
      private_key priv = private_key::regenerate( h );
      BOOST_CHECK( nullkey != priv );
      public_key pub = priv.get_public_key();

      pass += "1";
      multihash_type h2 = hash_str( CRYPTO_SHA2_256_ID, pass.c_str(), pass.size() );
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
   multihash_type mh;
   zero_hash( mh, CRYPTO_SHA2_256_ID );
   BOOST_CHECK( multihash::get_id( mh ) == CRYPTO_SHA2_256_ID );
   BOOST_CHECK( multihash::get_size( mh ) == 256/8 );

   zero_hash( mh, CRYPTO_RIPEMD160_ID );
   BOOST_CHECK( multihash::get_id( mh ) == CRYPTO_RIPEMD160_ID );
   BOOST_CHECK( multihash::get_size( mh ) == 160/8 );
}

BOOST_AUTO_TEST_SUITE_END()
