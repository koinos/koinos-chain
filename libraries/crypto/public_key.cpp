#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/crypto/openssl.hpp>

#include <koinos/pack/rt/util/base58.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include <secp256k1.h>

namespace koinos::crypto {

using koinos::exception::koinos_exception;
using namespace boost::multiprecision::literals;

// TODO: Move this to a more general purpose header
template< size_t N >
bool operator ==( const fl_blob< N >& a, const fl_blob< N >& b )
{
   return memcmp( a.data.data(), b.data.data(), N ) == 0;
}

template< size_t N >
bool operator !=( const fl_blob< N >& a, const fl_blob< N >& b )
{
   return !(a == b);
}

const secp256k1_context_t* _get_context()
{
   static secp256k1_context_t* ctx = secp256k1_context_create( SECP256K1_CONTEXT_VERIFY | SECP256K1_CONTEXT_SIGN );
   return ctx;
}

void _init_lib()
{
   static const secp256k1_context_t* ctx = _get_context();
   static int init_o = init_openssl();
   boost::ignore_unused(ctx, init_o);
}

static const public_key_data empty_pub{};
static const private_key_secret empty_priv{};


public_key::public_key() { _init_lib(); }

public_key::public_key( const public_key& pk ) : _key( pk._key ) { _init_lib(); }

public_key::public_key( public_key&& pk ) : _key( std::move( pk._key ) ) { _init_lib(); }

public_key::public_key( const public_key_data& dat )
{
   _init_lib();
   _key = dat;
}

public_key::public_key( const public_key_point_data& dat )
{
   _init_lib();
   const char* front = &dat.data[0];
   if( *front == 0 ){}
   else
   {
      EC_KEY *key = EC_KEY_new_by_curve_name( NID_secp256k1 );
      key = o2i_ECPublicKey( &key, (const unsigned char**)&front, sizeof(dat) );
      KOINOS_ASSERT( key, koinos_exception, "", () );
      EC_KEY_set_conv_form( key, POINT_CONVERSION_COMPRESSED );
      unsigned char* buffer = (unsigned char*) _key.data.data();
      i2o_ECPublicKey( key, &buffer ); // FIXME: questionable memory handling
      EC_KEY_free( key );
   }
}

public_key::public_key( const compact_signature& c, const multihash_type& digest, bool check_canonical )
{
   _init_lib();
   KOINOS_ASSERT( multihash::validate_sha256( digest ), koinos_exception, "", () );
   int nV = c.data[0];

   KOINOS_ASSERT( !(nV<27 || nV>=35), koinos_exception, "unable to reconstruct public key from signature", () );
   KOINOS_ASSERT( !check_canonical || is_canonical( c ), koinos_exception, "signature is not canonical", () );

   unsigned int pk_len;
   KOINOS_ASSERT( secp256k1_ecdsa_recover_compact( _get_context(), (unsigned char*) digest.digest.data.data(), (unsigned char*) c.data.data() + 1, (unsigned char*) _key.data.data(), (int*) &pk_len, 1, (*c.data.data() - 27) & 3 ),
      koinos_exception, "", () );
   KOINOS_ASSERT( pk_len == _key.data.size(), koinos_exception, "", () );
}

public_key::~public_key() {}

public_key_data public_key::serialize()const
{
   KOINOS_ASSERT( _key != empty_pub, koinos_exception, "", () );
   return _key;
}

public_key_point_data public_key::serialize_ecc_point()const
{
   KOINOS_ASSERT( _key != empty_pub, koinos_exception, "", () );
   public_key_point_data dat;
   unsigned int pk_len = _key.data.size();
   memcpy( dat.data.data(), _key.data.data(), pk_len );
   KOINOS_ASSERT( secp256k1_ec_pubkey_decompress( _get_context(), (unsigned char *) dat.data.data(), (int*) &pk_len ),
      koinos_exception, "", () );
   KOINOS_ASSERT( pk_len == dat.data.size(), koinos_exception, "", () );
   return dat;
}

public_key public_key::child( const multihash_type& offset )const
{
   KOINOS_ASSERT( multihash::validate_sha256( offset ), koinos_exception, "", () );
   encoder enc( CRYPTO_SHA2_256_ID );
   enc.write( _key.data.data(), _key.data.size() );
   enc.write( offset.digest.data.data(), offset.digest.data.size() );

   multihash_type result;
   enc.get_result( result );

   return add( result );
}

public_key public_key::add( const multihash_type& digest )const
{
   KOINOS_ASSERT( multihash::validate_sha256( digest ), koinos_exception, "", () );
   KOINOS_ASSERT( _key != empty_pub, koinos_exception, "", () );
   public_key_data new_key;
   memcpy( new_key.data.data(), _key.data.data(), sizeof(public_key_data) );
   KOINOS_ASSERT( secp256k1_ec_pubkey_tweak_add( _get_context(), (unsigned char*) new_key.data.data(), sizeof(public_key_data), (unsigned char*) digest.digest.data.data() ),
      koinos_exception, "", () );
   return public_key( new_key );
}

public_key& public_key::operator=( const public_key& pk )
{
   _key = pk._key;
   return *this;
}

public_key& public_key::operator=( public_key&& pk )
{
   _key = pk._key;
   return *this;
}

bool public_key::valid()const
{
   return _key != empty_pub;
}

unsigned int public_key::fingerprint() const
{
   public_key_data key = serialize();
   multihash_type sha256 = hash( CRYPTO_SHA2_256_ID, key.data.data(), key.data.size() );
   multihash_type ripemd160 = hash( CRYPTO_RIPEMD160_ID, sha256.digest.data.data(), sha256.digest.data.size() );
   unsigned char* fp = (unsigned char*) ripemd160.digest.data.data();
   return (fp[0] << 24) | (fp[1] << 16) | (fp[2] << 8) | fp[3];
}

bool public_key::is_canonical( const compact_signature& c )
{
   using boost::multiprecision::uint256_t;
   constexpr uint256_t n_2 =
      0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF5D576E7357A4501DDFE92F46681B20A0_cppui256;

   // BIP-0062 states that sig must be in [1,n/2], however because a sig of value 0 is an invalid
   // signature under all circumstances, the lower bound does not need checking
   return memcmp( c.data.data() + 33, &n_2, 32 ) <= 0;
}


std::string public_key::to_base58() const
{
   KOINOS_ASSERT( _key != empty_pub, koinos_exception, "", () );
   return to_base58( _key );
}

std::string public_key::to_base58( const public_key_data &key )
{
   uint32_t check = *((uint32_t*)hash( CRYPTO_SHA2_256_ID, key.data.data(), key.data.size() ).digest.data.data());
   assert( key.data.size() + sizeof(check) == 37 );
   fl_blob<37> d;
   memcpy( d.data.data(), key.data.data(), key.data.size() );
   memcpy( d.data.begin() + key.data.size(), (const char*)&check, sizeof(check) );
   std::string b58;
   koinos::pack::util::encode_base58( b58, d.data );
   return b58;
}

public_key public_key::from_base58( const std::string& b58 )
{
   fl_blob<37> d;
   koinos::pack::util::decode_base58( b58, d.data );
   KOINOS_ASSERT( b58.size() == sizeof(d), koinos_exception, "", () );

   public_key_data key;
   uint32_t check = *((uint32_t*)hash( CRYPTO_SHA2_256_ID, d.data.data(), key.data.size() ).digest.data.data());
   KOINOS_ASSERT( memcmp( (char*)&check, d.data.data() + sizeof(key), sizeof(check) ) == 0,
      koinos_exception, "", () );
   memcpy( (char*)key.data.data(), d.data.data(), sizeof(key) );
   return from_key_data( key );
}

public_key public_key::from_key_data( const public_key_data &data )
{
   return public_key( data );
}

} // koinos::crypto
