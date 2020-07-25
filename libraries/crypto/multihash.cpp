#include <koinos/crypto/multihash.hpp>

#define HASH_OFFSET (8)
#define HASH_MASK (~uint64_t(0)<<8)
#define SIZE_MASK ~HASH_MASK

#include <iostream>
#include <map>

namespace koinos::crypto {

const EVP_MD* get_evp_md( uint64_t code )
{
   static const std::map<uint64_t, const EVP_MD*> evp_md_map = {
      { CRYPTO_SHA1_ID, EVP_sha1() },
      { CRYPTO_SHA2_256_ID, EVP_sha256() },
      { CRYPTO_SHA2_512_ID, EVP_sha512() },
      { CRYPTO_RIPEMD160_ID, EVP_ripemd160() }
   };

   auto md_itr = evp_md_map.find( code );
   return md_itr != evp_md_map.end() ? md_itr->second : nullptr;
}

bool multihash_id_is_known( uint64_t id )
{
   switch ( id )
   {
      case CRYPTO_SHA1_ID:
      case CRYPTO_SHA2_256_ID:
      case CRYPTO_SHA2_512_ID:
      case CRYPTO_RIPEMD160_ID:
         return true;
         break;
      default:
         break;
   }
   return false;
}

encoder::encoder( uint64_t code, uint64_t size )
{
   static const uint64_t MAX_HASH_SIZE = std::min< uint64_t >(
      {std::numeric_limits< uint8_t >::max(),              // We potentially store the size in uint8_t value
       std::numeric_limits< unsigned int >::max(),         // We cast the size to unsigned int for openssl call
       EVP_MAX_MD_SIZE                                     // Max size supported by OpenSSL library
      });

   _code = code;
   if( size == 0 )
      size = multihash_standard_size( code );
   KOINOS_ASSERT( size <= MAX_HASH_SIZE, multihash_size_limit_exceeded,
      "Requested hash size ${size} is larger than max size ${max}", ("size", size)("max", MAX_HASH_SIZE) );

   _size = size;
   OpenSSL_add_all_digests();
   md = get_evp_md( code );
   KOINOS_ASSERT( md, unknown_hash_algorithm, "Unknown hash id ${i}", ("i", code) );
   mdctx = EVP_MD_CTX_create();
   EVP_DigestInit_ex( mdctx, md, NULL );
}

encoder::~encoder()
{
   if( mdctx ) EVP_MD_CTX_destroy( mdctx );
}

void encoder::write( const char* d, size_t len )
{
   EVP_DigestUpdate( mdctx, d, len );
};

void encoder::reset()
{
   if( mdctx ) EVP_MD_CTX_destroy( mdctx );
   if( md )
   {
      mdctx = EVP_MD_CTX_create();
      EVP_DigestInit_ex( mdctx, md, NULL );
   }
}

void encoder::get_result( variable_blob& v )
{
   unsigned int size = (unsigned int) _size;
   v.resize( _size );
   KOINOS_ASSERT(
      EVP_DigestFinal_ex(
         mdctx, (unsigned char*)( v.data() ), &size ),
      koinos::exception, "EVP_DigestFinal_ex returned failure" );
   KOINOS_ASSERT( size == _size,
      multihash_size_mismatch,
      "OpenSSL EVP_DigestFinal_ex returned hash size ${size}, does not match expected hash size ${_size}",
      ("size", size)("_size", _size) );
}

multihash hash_str( uint64_t code, const char* data, size_t len, uint64_t size )
{
   multihash result;
   encoder e( code, size );
   e.write( data, len );
   e.get_result( result );
   return result;
}


multihash hash_str_like( const multihash& old, const char* data, size_t len )
{
   return hash_str( old.id, data, len, old.digest.size() );
}

multihash hash_blob( uint64_t code, const variable_blob& value, uint64_t size )
{
   return hash_str( code, value.data(), value.size(), size );
}

multihash hash_blob_like( const multihash& old, const variable_blob& value )
{
   return hash_str( old.id, value.data(), value.size(), old.digest.size() );
}

multihash empty_hash( uint64_t code, uint64_t size )
{
   char c;
   return hash_str( code, &c, 0, size );
}

multihash empty_hash_like( const multihash& old )
{
   char c;
   return hash_str( old.id, &c, 0, old.digest.size() );
}

multihash zero_hash( uint64_t code, uint64_t size )
{
   multihash result;
   result.id = code;

   if ( !size )
      size = multihash_standard_size( code );

   result.digest.resize( size );
   std::memset( result.digest.data(), 0, size );
   return result;
}

multihash zero_hash_like( const multihash& old )
{
   return zero_hash( old.id, old.digest.size() );
}

multihash_vector to_multihash_vector( const std::vector< multihash >& mh_in )
{
   multihash_vector mhv_out;
   const size_t n = mh_in.size();
   KOINOS_ASSERT( n > 0, multihash_size_mismatch, "Input vector cannot be empty" );

   mhv_out.digests.resize( n );
   mhv_out.id = mh_in[0].id;

   for ( size_t i=0; i<n; i++ )
   {
      mhv_out.digests[i] = mh_in[i].digest;
      KOINOS_ASSERT( mh_in[i].id == mhv_out.id,
         multihash_vector_mismatch,
         "Heterogenous multihash_vector, expected hash_id == ${h_out}, got hash_id == ${h_in}",
         ("h_out", mhv_out.id)("h_in", mh_in[i].id) );
   }
   return mhv_out;
}

std::vector< multihash > from_multihash_vector( const multihash_vector& mhv_in )
{
   std::vector< multihash > mh_out;
   const size_t n = mhv_in.digests.size();
   mh_out.resize( n );
   for ( size_t i = 0; i < n; i++ )
   {
      mh_out[i].id = mhv_in.id;
      mh_out[i].digest = mhv_in.digests[i];
   }
   return mh_out;
}

void merkle_hash_leaves( std::vector< multihash >& hashes, uint64_t code, uint64_t size )
{
   size_t n_hashes = hashes.size();
   if ( n_hashes == 0 )
   {
      hashes.resize(1);
      // Corner case:  Merkle root of empty sequence is H("")
      hashes[0] = empty_hash( code, size );
      return;
   }

   encoder enc( code, size );
   while ( n_hashes > 1 )
   {
      size_t num_pairs = n_hashes >> 1;
      for( size_t i=0; i < num_pairs; i++ )
      {
         enc.reset();
         enc.write( hashes[i*2  ].digest.data(), hashes[i*2  ].digest.size() );
         enc.write( hashes[i*2+1].digest.data(), hashes[i*2+1].digest.size() );
         enc.get_result( hashes[i] );
      }
      if( ( n_hashes & 1 ) != 0 )
      {
         hashes[num_pairs] = hashes[n_hashes-1];
         n_hashes = num_pairs+1;
      }
      else
      {
         n_hashes = num_pairs;
      }
   }
}

void merkle_hash_leaves_like( std::vector< multihash >& hashes, const multihash& old )
{
   merkle_hash_leaves( hashes, old.id, old.digest.size() );
}

multihash merkle_hash( uint64_t code, const std::vector< variable_blob >& values, uint64_t size )
{
   std::size_t n_hashes = values.size();
   if ( n_hashes == 0 )
   {
      // Corner case:  Merkle root of empty sequence is H("")
      return crypto::empty_hash( code, size );
   }

   std::vector< multihash > hashes( n_hashes );
   for ( size_t i = 0; i < n_hashes; i++ )
   {
      hashes[i] = crypto::hash_str( code, values[i].data(), values[i].size(), size );
   }

   merkle_hash_leaves( hashes, code, size );
   return hashes[0];
}

multihash merkle_hash_like( const multihash& old, const std::vector< variable_blob >& values )
{
   return merkle_hash( old.id, values, old.digest.size() );
}

} // koinos::crypto
