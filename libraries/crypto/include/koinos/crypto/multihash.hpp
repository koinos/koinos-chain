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

namespace koinos { namespace protocol {

bool operator ==( const multihash_type& mha, const multihash_type& mhb );
bool operator !=( const multihash_type& mha, const multihash_type& mhb );
bool operator <( const multihash_type& mha, const multihash_type& mhb );
bool operator <=( const multihash_type& mha, const multihash_type& mhb );
bool operator >( const multihash_type& mha, const multihash_type& mhb );
bool operator >=( const multihash_type& mha, const multihash_type& mhb );

} // protocol

namespace crypto {

using koinos::protocol::multihash_type;
using koinos::protocol::multihash_vector;
using koinos::protocol::vl_blob;

DECLARE_KOINOS_EXCEPTION( unknown_hash_algorithm );
DECLARE_KOINOS_EXCEPTION( multihash_size_mismatch );
DECLARE_KOINOS_EXCEPTION( multihash_size_limit_exceeded );

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
} // multihash

struct encoder
{
   encoder( uint64_t code, uint64_t size = 0 );
   ~encoder();

   void write( const char* d, size_t len );
   void put( char c ) { write( &c, 1 ); }
   void reset();
   void get_result( vl_blob& v );
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
multihash_type hash( uint64_t code, T& t, size_t size = 0 )
{
   encoder e( code, size );
   koinos::pack::to_binary( e, t );
   multihash_type mh;
   multihash::set_id( mh, code );
   e.get_result( mh.digest );

   multihash::set_size( mh, result_size );
   return mh;
};

multihash_type hash( uint64_t code, const char* data, size_t len, size_t size = 0 );

void zero_hash( multihash_type& mh, uint64_t code, uint64_t size = 0 );

inline constexpr uint64_t get_standard_size( uint64_t code )
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

} } // koinos::crypto
