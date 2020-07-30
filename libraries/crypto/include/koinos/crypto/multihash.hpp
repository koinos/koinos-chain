#pragma once
#include <koinos/exception.hpp>

#include <koinos/pack/rt/basetypes.hpp>
#include <koinos/pack/rt/binary.hpp>

#include <openssl/evp.h>

/* Multicodec IDs for hash algorithms
 * https://github.com/multiformats/multicodec/blob/master/table.csv
 */
#define CRYPTO_SHA1_ID        uint64_t(0x11)
#define CRYPTO_SHA2_256_ID    uint64_t(0x12)
#define CRYPTO_SHA2_512_ID    uint64_t(0x13)
#define CRYPTO_RIPEMD160_ID   uint64_t(0x1053)

namespace koinos::crypto {

using koinos::types::multihash;
using koinos::types::multihash_vector;
using koinos::types::variable_blob;

KOINOS_DECLARE_EXCEPTION( unknown_hash_algorithm );
KOINOS_DECLARE_EXCEPTION( multihash_size_mismatch );
KOINOS_DECLARE_EXCEPTION( multihash_size_limit_exceeded );
KOINOS_DECLARE_EXCEPTION( multihash_vector_mismatch );

inline bool multihash_is_zero( const multihash& mh )
{
   return std::all_of( mh.digest.begin(), mh.digest.end(), []( char c ) { return (c == 0); } );
}

inline uint64_t multihash_standard_size( uint64_t id )
{
   switch( id )
   {
      case CRYPTO_SHA1_ID:
         return 20;
      case CRYPTO_SHA2_256_ID:
         return 32;
      case CRYPTO_SHA2_512_ID:
         return 64;
      case CRYPTO_RIPEMD160_ID:
         return 20;
      default:
         KOINOS_ASSERT( false, unknown_hash_algorithm, "Unknown hash id ${i}", ("i", id) );
   }
}

constexpr bool multihash_id_is_known( uint64_t id )
{
   switch ( id )
   {
      case CRYPTO_SHA1_ID:
      case CRYPTO_SHA2_256_ID:
      case CRYPTO_SHA2_512_ID:
      case CRYPTO_RIPEMD160_ID:
         return true;
   }
   return false;
}

struct encoder
{
   encoder( uint64_t code, uint64_t size = 0 );
   ~encoder();

   void write( const char* d, size_t len );
   void put( char c ) { write( &c, 1 ); }
   void reset();
   void get_result( variable_blob& v );
   inline void get_result( multihash& mh )
   {
      get_result( mh.digest );
      mh.id = _code;
   }

   private:
      const EVP_MD* md = nullptr;
      EVP_MD_CTX* mdctx = nullptr;
      uint64_t _code, _size;
};

template< typename T >
inline multihash hash( uint64_t code, const T& t, uint64_t size = 0 )
{
   multihash result;
   encoder e( code, size );
   koinos::pack::to_binary( e, t );
   e.get_result( result );
   return result;
}

template< typename T >
multihash hash_like( const multihash& old, const T& t )
{
   return hash< T >( old.id, t, old.digest.size() );
}

multihash hash_str( uint64_t code, const char* data, size_t len, uint64_t size = 0 );
multihash hash_str_like( const multihash& old, const char* data, size_t len );

multihash hash_blob( uint64_t code, const variable_blob& value, uint64_t size = 0 );
multihash hash_blob_like( const multihash& old, const variable_blob& value );

multihash zero_hash( uint64_t code, uint64_t size = 0 );
multihash zero_hash_like( const multihash& old );

multihash empty_hash( uint64_t code, uint64_t size = 0 );
multihash empty_hash_like( const multihash& old );

multihash_vector to_multihash_vector( const std::vector< multihash >& mh_in );
std::vector< multihash > from_multihash_vector( const multihash_vector& mhv_in );

multihash merkle_hash( uint64_t code, const std::vector< variable_blob >& values, uint64_t size = 0 );
multihash merkle_hash_like( const multihash& old, const std::vector< variable_blob >& values );

/**
 * Compute Merkle root given leaf hash values.
 * Computation is in-place, result is stored in hashes[0].
 * Other hashes values are destroyed.
 */
void merkle_hash_leaves( std::vector< multihash >& hashes, uint64_t code, uint64_t size = 0 );
void merkle_hash_leaves_like( std::vector< multihash >& hashes, const multihash& old );

} // koinos::crypto
