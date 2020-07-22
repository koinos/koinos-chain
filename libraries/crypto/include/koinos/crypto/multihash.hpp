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

bool is_known_code( uint64_t code );

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
      mh.digest.resize( _size );
   }

   private:
      const EVP_MD* md = nullptr;
      EVP_MD_CTX* mdctx = nullptr;
      uint64_t _code, _size;
};

template< typename T >
inline void hash( multihash& result, uint64_t code, const T& t, uint64_t size = 0 )
{
   encoder e( code, size );
   koinos::pack::to_binary( e, t );
   e.get_result( result );
}

template< typename T >
inline multihash hash( uint64_t code, const T& t, uint64_t size = 0 )
{
   multihash mh;
   hash( mh, code, t, size );
   return mh;
}

/**
 * Hash using the same algorithm as an existing multihash_type object.
 */
template< typename T >
inline void hash_like( multihash& result, const multihash& old, const T& t )
{
   hash<T>( result, old.id, t, old.digest.size() );
}

void hash_str( multihash& result, uint64_t code, const char* data, size_t len, uint64_t size = 0 );
multihash hash_str( uint64_t code, const char* data, size_t len, uint64_t size = 0 );
void hash_str_like( multihash& result, const multihash& old, const char* data, size_t len );

void hash_blob( multihash& result, uint64_t code, const variable_blob& value, uint64_t size = 0 );
void hash_blob_like( multihash& result, const multihash& old, const variable_blob& value );

void zero_hash( multihash& mh, uint64_t code, uint64_t size = 0 );
multihash zero_hash( uint64_t code, uint64_t size = 0 );
void zero_hash_like( multihash& result, const multihash& old );

void empty_hash( multihash& mh, uint64_t code, uint64_t size = 0 );
void empty_hash_like( multihash& result, const multihash& old );

void to_multihash_vector( multihash_vector& mhv_out, const std::vector< multihash >& mh_in );
void from_multihash_vector( std::vector< multihash >& mh_out, const multihash_vector& mhv_in );

inline uint64_t get_standard_size( uint64_t code )
{
   switch( code )
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
         KOINOS_ASSERT( false, unknown_hash_algorithm, "Unknown hash id ${i}", ("i", code) );
   }
}

void merkle_hash( multihash& result, uint64_t code, const std::vector< variable_blob >& values, uint64_t size = 0 );
void merkle_hash_like( multihash& result, const multihash& old, const std::vector< variable_blob >& values );

/**
 * Compute Merkle root given leaf hash values.
 * Computation is in-place, result is stored in hashes[0].
 * Other hashes values are destroyed.
 */
void merkle_hash_leaves( std::vector< multihash >& hashes, uint64_t code, uint64_t size = 0 );
void merkle_hash_leaves_like( std::vector< multihash >& hashes, const multihash& old );

} // koinos::crypto
