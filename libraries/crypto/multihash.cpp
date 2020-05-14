#include <koinos/crypto/multihash.hpp>

#define HASH_OFFSET (8)
#define HASH_MASK (~uint64_t(0)<<8)
#define SIZE_MASK ~HASH_MASK


namespace koinos::crypto {

   const EVP_MD* get_evp_md( uint64_t code )
   {
      static const std::map<uint64_t, const EVP_MD*> evp_md_map = {
         { CRYPTO_SHA1_ID, EVP_sha1() },
         { CRYPTO_SHA2_256_ID, EVP_sha256() }
      };

      auto md_itr = evp_md_map.find( code );
      return md_itr != evp_md_map.end() ? md_itr->second : nullptr;
   }

   void multihash::set_id( multihash_type& mh, uint64_t code )
   {
      mh.hash_id &= SIZE_MASK;
      mh.hash_id |= (code << HASH_OFFSET);
   }

   void multihash::set_id( multihash_vector& mhv, uint64_t code )
   {
      mhv.hash_id &= SIZE_MASK;
      mhv.hash_id |= (code << HASH_OFFSET);
   }

   uint64_t multihash::id( multihash_type& mh )
   {
      return (mh.hash_id | HASH_MASK) >> HASH_OFFSET;
   }

   uint64_t multihash::id( multihash_vector& mhv )
   {
      return (mhv.hash_id | HASH_MASK) >> HASH_OFFSET;
   }

   void multihash::set_size( multihash_type& mh, uint64_t size )
   {
      mh.hash_id &= HASH_MASK;
      mh.hash_id |= std::min( size, SIZE_MASK );
   }

   void multihash::set_size( multihash_vector& mhv, uint64_t size )
   {
      mhv.hash_id &= HASH_MASK;
      mhv.hash_id |= std::min( size, SIZE_MASK );
   }

   uint64_t multihash::size( multihash_type& mh )
   {
      return mh.hash_id & SIZE_MASK;
   }

   uint64_t multihash::size( multihash_vector& mhv )
   {
      return mhv.hash_id & SIZE_MASK;
   }

   bool multihash::validate( multihash_type& mh )
   {
      return get_evp_md( id( mh ) ) != nullptr
         && size( mh ) == mh.digest.data.size();
   }

   bool multihash::validate( multihash_vector& mhv )
   {
      if( get_evp_md( id( mhv ) ) == nullptr ) return false;

      uint64_t s = size( mhv );

      for( auto d : mhv.digests )
      {
         if( d.data.size() != s ) return false;
      }

      return true;
   }

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
      size = EVP_DigestFinal_ex( mdctx, (unsigned char*)( v.data.data() ), (unsigned int*)&size );
      v.data.resize( size );
      return size;
   }

} // koinos::crypto
