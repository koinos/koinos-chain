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

encoder::encoder( uint64_t code )
{
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

size_t encoder::get_result( vl_blob& v, size_t size )
{
   if( !size ) size = EVP_MAX_MD_SIZE;
   v.data.resize( size );
   KOINOS_ASSERT(
      EVP_DigestFinal_ex(
         mdctx, (unsigned char*)( v.data.data() ), (unsigned int*)&size ),
      koinos::exception::koinos_exception, "", () );
   v.data.resize( size );
   return size;
}

multihash_type hash( uint64_t code, const char* data, size_t len, size_t size )
{
   encoder e( code );
   e.write( data, len );
   multihash_type mh;
   multihash::set_id( mh, code );
   size_t hash_size = e.get_result( mh.digest, size );

   if( size )
      KOINOS_ASSERT( size == hash_size, multihash_size_mismatch, "OpenSSL Hash size does not match expected multihash size", () );
   KOINOS_ASSERT( hash_size <= std::numeric_limits< uint8_t >::max(), multihash_size_limit_exceeded, "Multihash size exceeds max", () );

   multihash::set_size( mh, hash_size );
   return mh;
}

} } // koinos::crypto
