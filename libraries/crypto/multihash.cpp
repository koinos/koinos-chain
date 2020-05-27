#include <koinos/crypto/multihash.hpp>

#define HASH_OFFSET (8)
#define HASH_MASK (~uint64_t(0)<<8)
#define SIZE_MASK ~HASH_MASK

#include <iostream>
#include <map>

namespace koinos { namespace protocol {

bool operator ==( const multihash_type& mha, const multihash_type& mhb )
{
   return ( koinos::crypto::multihash::validate( mha ) && koinos::crypto::multihash::validate( mhb ) )
       && ( koinos::crypto::multihash::get_id( mha )   == koinos::crypto::multihash::get_id( mhb ) )
       && ( koinos::crypto::multihash::get_size( mha ) == koinos::crypto::multihash::get_size( mhb ) )
       && ( memcmp( mha.digest.data.data(), mhb.digest.data.data(), mhb.digest.data.size() ) == 0 );
}

bool operator !=( const multihash_type& mha, const multihash_type& mhb )
{
   return !(mha == mhb);
}

bool operator <( const multihash_type& mha, const multihash_type& mhb )
{
   int64_t res = (int64_t)mha.hash_id - (int64_t)mhb.hash_id;
   if( res < 0 ) return true;
   if( res > 0 ) return false;
   res = mha.digest.data.size() - mhb.digest.data.size();
   if( res < 0 ) return true;
   if( res > 0 ) return false;
   return memcmp( mha.digest.data.data(), mhb.digest.data.data(), mha.digest.data.size() ) < 0;
}

bool operator <=( const multihash_type& mha, const multihash_type& mhb )
{
   int64_t res = (int64_t)mha.hash_id - (int64_t)mhb.hash_id;
   if( res < 0 ) return true;
   if( res > 0 ) return false;
   res = mha.digest.data.size() - mhb.digest.data.size();
   if( res < 0 ) return true;
   if( res > 0 ) return false;
   return memcmp( mha.digest.data.data(), mhb.digest.data.data(), mha.digest.data.size() ) <= 0;
}

bool operator >( const multihash_type& mha, const multihash_type& mhb )
{
   return !(mha <= mhb);
}

bool operator >=( const multihash_type& mha, const multihash_type& mhb )
{
   return !(mha < mhb);
}

} // protocol

namespace crypto {

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

namespace multihash {
void set_id( multihash_type& mh, uint64_t code )
{
   mh.hash_id &= SIZE_MASK;
   mh.hash_id |= (code << HASH_OFFSET);
}

void set_id( multihash_vector& mhv, uint64_t code )
{
   mhv.hash_id &= SIZE_MASK;
   mhv.hash_id |= (code << HASH_OFFSET);
}

uint64_t get_id( const multihash_type& mh )
{
   return mh.hash_id >> HASH_OFFSET;
}

uint64_t get_id( const multihash_vector& mhv )
{
   return mhv.hash_id >> HASH_OFFSET;
}

void set_size( multihash_type& mh, uint64_t size )
{
   mh.hash_id &= HASH_MASK;
   mh.hash_id |= std::min( size, SIZE_MASK );
}

void set_size( multihash_vector& mhv, uint64_t size )
{
   mhv.hash_id &= HASH_MASK;
   mhv.hash_id |= std::min( size, SIZE_MASK );
}

uint64_t get_size( const multihash_type& mh )
{
   return mh.hash_id & SIZE_MASK;
}

uint64_t get_size( const multihash_vector& mhv )
{
   return mhv.hash_id & SIZE_MASK;
}

bool validate( const multihash_type& mh, uint64_t code, uint64_t size )
{
   if( code && ( get_id( mh ) != code ) ) return false;
   if( size && ( get_size( mh ) != size ) ) return false;
   return get_evp_md( get_id( mh ) ) != nullptr
      && get_size( mh ) == mh.digest.data.size();
}

bool validate( const multihash_vector& mhv, uint64_t code, uint64_t size )
{
   if( code && get_id( mhv ) != code ) return false;
   if( size && get_size( mhv ) != size ) return false;
   if( get_evp_md( get_id( mhv ) ) == nullptr ) return false;

   uint64_t s = get_size( mhv );

   for( auto d : mhv.digests )
   {
      if( d.data.size() != s ) return false;
   }

   return true;
}

} // multihash

encoder::encoder( uint64_t code, uint64_t size )
{
   static const uint64_t MAX_HASH_SIZE = std::min< uint64_t >(
      {std::numeric_limits< uint8_t >::max(),              // We potentially store the size in uint8_t value
       std::numeric_limits< unsigned int >::max(),         // We cast the size to unsigned int for openssl call
       EVP_MAX_MD_SIZE                                     // Max size supported by OpenSSL library
      });

   _code = code;
   if( size == 0 )
      size = get_standard_size( code );
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

void encoder::get_result( vl_blob& v )
{
   unsigned int size = (unsigned int) _size;
   v.data.resize( _size );
   KOINOS_ASSERT(
      EVP_DigestFinal_ex(
         mdctx, (unsigned char*)( v.data.data() ), &size ),
      koinos::exception::koinos_exception, "EVP_DigestFinal_ex returned failure", () );
   KOINOS_ASSERT( size == _size,
      multihash_size_mismatch,
      "OpenSSL EVP_DigestFinal_ex returned hash size ${size}, does not match expected hash size ${_size}",
      ("size", size)("_size", _size) );
}

multihash_type hash_str( uint64_t code, const char* data, size_t len, size_t size )
{
   encoder e( code );
   e.write( data, len );
   multihash_type mh;
   e.get_result( mh );
   return mh;
}

void zero_hash( multihash_type& mh, uint64_t code, uint64_t size )
{
   multihash::set_id( mh, code );
   if( size == 0 )
      size = get_standard_size( code );
   multihash::set_size( mh, size );
   mh.digest.data.resize( size );
   std::memset( mh.digest.data.data(), 0, size );
}

void to_multihash_vector( multihash_vector& mhv_out, const std::vector< multihash_type >& mh_in )
{
   const size_t n = mh_in.size();
   KOINOS_ASSERT( n > 0, multihash_size_mismatch, "Input vector cannot be empty" );

   mhv_out.digests.resize( n );
   mhv_out.hash_id = mh_in[0].hash_id;

   for( size_t i=0; i<n; i++ )
   {
      mhv_out.digests[i] = mh_in[i].digest;
      KOINOS_ASSERT( mh_in[i].hash_id == mhv_out.hash_id,
         multihash_vector_mismatch,
         "Heterogenous multihash_vector, expected hash_id == ${h_out}, got hash_id == ${h_in}",
         ("h_out", mhv_out.hash_id)("h_in", mh_in[i].hash_id) );
   }
}

void from_multihash_vector( std::vector< multihash_type >& mh_out, const multihash_vector& mhv_in )
{
   const size_t n = mhv_in.digests.size();
   mh_out.resize( n );
   for( size_t i=0; i<n; i++ )
   {
      mh_out[i].hash_id = mhv_in.hash_id;
      mh_out[i].digest = mhv_in.digests[i];
   }
}

} } // koinos::crypto
