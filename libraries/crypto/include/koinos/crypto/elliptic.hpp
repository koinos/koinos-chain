#pragma once
#include <koinos/pack/rt/basetypes.hpp>

#include <openssl/ec.h>

#include <secp256k1.h>

namespace koinos::crypto {

   using koinos::protocol::multihash_type;
   using koinos::protocol::fl_blob;

   typedef fl_blob<65>     compact_signature;
   typedef fl_blob<33>     public_key_data;
   typedef fl_blob<65>     public_key_point_data;  ///< the full non-compressed version of the ECC point
   typedef multihash_type  private_key_secret;

   /**
    *  @class public_key
    *  @brief contains only the public point of an elliptic curve key.
    */
   class public_key
   {
      public:
         public_key();
         public_key(const public_key& k);
         public_key( public_key&& pk );
         public_key( const public_key_data& v );
         public_key( const public_key_point_data& v );
         public_key( const compact_signature& c, const multihash_type& digest, bool check_canonical = true );

         ~public_key();

         public_key_data serialize()const;
         public_key_point_data serialize_ecc_point()const;

         operator public_key_data()const { return serialize(); }

         public_key child( const multihash_type& offset )const;

         /** Computes new pubkey = regenerate(offset).pubkey + old pubkey
         *                      = offset * G + 1 * old pubkey ?! */
         public_key add( const multihash_type& offset )const;

         bool valid()const;

         public_key& operator =( public_key&& pk );
         public_key& operator =( const public_key& pk );

         inline friend bool operator ==( const public_key& a, const public_key& b )
         {
            return memcmp( a.serialize().data.data(), b.serialize().data.data(), sizeof(public_key_data) ) == 0;
         }

         inline friend bool operator !=( const public_key& a, const public_key& b )
         {
            return !(a == b);
         }

         // Allows to convert current public key object into base58 number.
         std::string to_base58() const;
         static std::string to_base58( const public_key_data &key );
         static public_key from_base58( const std::string& b58 );

         unsigned int fingerprint() const;

         static bool is_canonical( const compact_signature& c );

      private:
         friend class private_key;
         static public_key from_key_data( const public_key_data& v );

         public_key_data _key;
   };

   /**
    *  @class private_key
    *  @brief an elliptic curve private key.
    */
   class private_key
   {
      public:
         private_key();
         private_key( private_key&& pk );
         private_key( const private_key& pk );
         ~private_key();

         private_key& operator=( private_key&& pk );
         private_key& operator=( const private_key& pk );

         static private_key generate();
         static private_key regenerate( const multihash_type& secret );

         private_key child( const multihash_type& offset )const;

         /**
         *  This method of generation enables creating a new private key in a deterministic manner relative to
         *  an initial seed.   A public_key created from the seed can be multiplied by the offset to calculate
         *  the new public key without having to know the private key.
         */
         static private_key generate_from_seed( const multihash_type& seed, const multihash_type& offset = multihash_type() );

         private_key_secret get_secret()const; // get the private key secret

         operator private_key_secret ()const { return get_secret(); }

         /**
         *  Given a public key, calculatse a 512 bit shared secret between that
         *  key and this private key.
         */
         multihash_type get_shared_secret( const public_key& pub )const;

         compact_signature sign_compact( const multihash_type& digest )const;

         public_key get_public_key()const;

         inline friend bool operator==( const private_key& a, const private_key& b )
         {
            return memcmp( a.get_secret().digest.data.data(), b.get_secret().digest.data.data(), 32 ) == 0;
         }

         inline friend bool operator!=( const private_key& a, const private_key& b )
         {
            return !(a == b);
         }

         inline friend bool operator<( const private_key& a, const private_key& b )
         {
            return memcmp( a.get_secret().digest.data.data(), b.get_secret().digest.data.data(), 32 ) < 0;
         }

         unsigned int fingerprint() const { return get_public_key().fingerprint(); }

      private:
         private_key( EC_KEY* k );
         static multihash_type get_secret( const EC_KEY * const k );
         private_key_secret _key;
   };

} // koinos::crypto
