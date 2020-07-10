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

namespace koinos { namespace types {

bool operator ==( const multihash_type& mha, const multihash_type& mhb );
bool operator !=( const multihash_type& mha, const multihash_type& mhb );
bool operator <( const multihash_type& mha, const multihash_type& mhb );
bool operator <=( const multihash_type& mha, const multihash_type& mhb );
bool operator >( const multihash_type& mha, const multihash_type& mhb );
bool operator >=( const multihash_type& mha, const multihash_type& mhb );

} // types

namespace crypto {

using koinos::types::multihash_type;
using koinos::types::multihash_vector;
using koinos::types::variable_blob;

KOINOS_DECLARE_EXCEPTION( unknown_hash_algorithm );
KOINOS_DECLARE_EXCEPTION( multihash_size_mismatch );
KOINOS_DECLARE_EXCEPTION( multihash_size_limit_exceeded );
KOINOS_DECLARE_EXCEPTION( multihash_vector_mismatch );

namespace multihash
{
   void set_id( multihash_type& mh,    uint64_t code );
   void set_id( multihash_vector& mhv, uint64_t code );

   uint64_t get_id( const multihash_type& mh );
   uint64_t get_id( const multihash_vector& mhv );

   void set_size( multihash_type& mh,    uint64_t size );
   void set_size( multihash_vector& mhv, uint64_t size );

   uint64_t get_size( const multihash_type& mh );
   uint64_t get_size( const multihash_vector& mhv );

   bool validate( const multihash_type& mh,    uint64_t code = 0, uint64_t size = 0 );
   bool validate( const multihash_vector& mhv, uint64_t code = 0, uint64_t size = 0 );

   inline bool validate_sha256( const multihash_type& mh )    { return validate( mh,  CRYPTO_SHA2_256_ID, 32 ); }
   inline bool validate_sha256( const multihash_vector& mhv ) { return validate( mhv, CRYPTO_SHA2_256_ID, 32 ); }

   inline bool is_zero( const multihash_type& mh )
   {
      return std::all_of( mh.digest.begin(), mh.digest.end(), []( char c ) { return (c == 0); } );
   }

   bool is_known_code( uint64_t code );
} // multihash

struct encoder
{
   encoder( uint64_t code, uint64_t size = 0 );
   ~encoder();

   void write( const char* d, size_t len );
   void put( char c ) { write( &c, 1 ); }
   void reset();
   void get_result( variable_blob& v );
   inline void get_result( multihash_type& mh )
   {
      get_result( mh.digest );
      multihash::set_id( mh, _code );
      multihash::set_size( mh, _size );
   }

   private:
      const EVP_MD* md = nullptr;
      EVP_MD_CTX* mdctx = nullptr;
      uint64_t _code, _size;
};

template< typename T >
inline void hash( multihash_type& result, uint64_t code, const T& t, uint64_t size = 0 )
{
   encoder e(code, size);
   koinos::pack::to_binary( e, t );
   e.get_result( result );
}

template< typename T >
inline multihash_type hash( uint64_t code, const T& t, uint64_t size = 0 )
{
   multihash_type mh;
   hash( mh, code, t, size );
   return mh;
}

/**
 * Hash using the same algorithm as an existing multihash_type object.
 */
template< typename T >
inline void hash_like( multihash_type& result, const multihash_type& old, const T& t )
{
   hash<T>( result, multihash::get_id( old ), t, multihash::get_size( old ) );
}

void hash_str( multihash_type& result, uint64_t code, const char* data, size_t len, uint64_t size = 0 );
multihash_type hash_str( uint64_t code, const char* data, size_t len, uint64_t size = 0 );
void hash_str_like( multihash_type& result, const multihash_type& old, const char* data, size_t len );

void hash_blob( multihash_type& result, uint64_t code, const variable_blob& value, uint64_t size = 0 );
void hash_blob_like( multihash_type& result, const multihash_type& old, const variable_blob& value );

void zero_hash( multihash_type& mh, uint64_t code, uint64_t size = 0 );
multihash_type zero_hash( uint64_t code, uint64_t size = 0 );
void zero_hash_like( multihash_type& result, const multihash_type& old );

void empty_hash( multihash_type& mh, uint64_t code, uint64_t size = 0 );
void empty_hash_like( multihash_type& result, const multihash_type& old );

void to_multihash_vector( multihash_vector& mhv_out, const std::vector< multihash_type >& mh_in );
void from_multihash_vector( std::vector< multihash_type >& mh_out, const multihash_vector& mhv_in );

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

void merkle_hash( multihash_type& result, uint64_t code, const std::vector< variable_blob >& values, uint64_t size = 0 );
void merkle_hash_like( multihash_type& result, const multihash_type& old, const std::vector< variable_blob >& values );

/**
 * Compute Merkle root given leaf hash values.
 * Computation is in-place, result is stored in hashes[0].
 * Other hashes values are destroyed.
 */
void merkle_hash_leaves( std::vector< multihash_type >& hashes, uint64_t code, uint64_t size = 0 );
void merkle_hash_leaves_like( std::vector< multihash_type >& hashes, const multihash_type& old );

} } // koinos::crypto
