#pragma once
#include <koinos/exception.hpp>

#include <koinos/pack/rt/basetypes.hpp>
#include <koinos/pack/rt/binary.hpp>

#include <openssl/evp.h>

#define CRYPTO_SHA1_ID     uint64_t(0x11)
#define CRYPTO_SHA2_256_ID uint64_t(0x12)

namespace koinos::crypto {

using koinos::protocol::multihash_type;
using koinos::protocol::multihash_vector;
using koinos::protocol::vl_blob;

DECLARE_KOINOS_EXCEPTION( unknown_hash_algorithm );
DECLARE_KOINOS_EXCEPTION( multihash_size_mismatch );
DECLARE_KOINOS_EXCEPTION( multihash_size_limit_exceeded );

struct multihash
{
   multihash() = delete;

   static void set_id( multihash_type& mh, uint64_t code );
   static void set_id( multihash_vector& mhv, uint64_t code );

   static uint64_t id( multihash_type& mh );
   static uint64_t id( multihash_vector& mhv );

   static void set_size( multihash_type& mh, uint64_t size );
   static void set_size( multihash_vector& mhv, uint64_t size );

   static uint64_t size( multihash_type& mh );
   static uint64_t size( multihash_vector& mhv );

   static bool validate( multihash_type& mh );
   static bool validate( multihash_vector& mhv );
};

struct encoder
{
   encoder( uint64_t code );
   ~encoder();

   void write( const char* d, size_t len );
   void put( char c ) { write( &c, 1 ); }
   void reset();
   size_t get_result( vl_blob& v, size_t size = 0 );

   private:
      const EVP_MD* md = nullptr;
      EVP_MD_CTX* mdctx = nullptr;
};

template< typename T >
multihash_type hash( uint64_t code, T& t, size_t size = 0 )
{
   encoder e( code );
   koinos::pack::to_binary( e, t );
   multihash_type mh;
   multihash::set_id( mh, code );
   size_t hash_size = e.get_result( mh.digest, size );

   if( size )
      KOINOS_ASSERT( size == hash_size, multihash_size_mismatch, "OpenSSL Hash size does not match expected multihash size", () );
   KOINOS_ASSERT( hash_size <= std::numeric_limits< uint8_t >::max(), multihash_size_limit_exceeded, "Multihash size exceeds max", () );

   multihash::set_size( mh, hash_size );
   return mh;
};

template< typename Iter >
multihash_vector hash( uint64_t code, Iter first, Iter last, size_t size = 0 )
{
   encoder e( code );
   multihash_vector mhv;
   multihash::set_id( mhv, code );

   for(; first != last; ++first )
   {
      koinos::pack::to_binary( e, *first );
      mhv.digests.emplace_back();
      size_t hash_size = e.get_result( mhv.digests.back(), size );

      if( multihash::size( mhv ) == 0 )
      {
         if( size )
            KOINOS_ASSERT( size == hash_size, multihash_size_mismatch, "OpenSSL Hash size does not match expected multihash size", () );
         KOINOS_ASSERT( hash_size <= std::numeric_limits< uint16_t >::max(), multihash_size_limit_exceeded, "Multihash size exceeds max", () );

         multihash::set_size( mhv, hash_size );
      }
      else
      {
         KOINOS_ASSERT( hash_size == multihash::size( mhv ), multihash_size_mismatch, "OpenSSL Hash size does not match expected multihash size", () );
      }
   }

   return mhv;
};

template< typename T >
bool add_hash( multihash_vector& mhv, T& t )
{
   encoder e( multihash::id( mhv ) );
   koinos::pack::to_binary( e, t );
   mhv.digests.emplace_back();
   size_t hash_size = e.get_result( mhv.digests.back() );
   if( hash_size == multihash::size( mhv ) )
      return true;

   mhv.digests.pop_back();
   return false;
};

} // koinos::crypto
