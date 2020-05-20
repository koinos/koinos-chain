#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/crypto/openssl.hpp>

#include <koinos/pack/rt/util/base58.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include <secp256k1_recovery.h>

#include <iostream>

namespace koinos::crypto {

template< typename Blob >
std::string hex_string( const Blob& b )
{
   static const char hex[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

   std::stringstream ss;

   for( auto c : b.data )
      ss << hex[(c & 0xF0) >> 4] << hex[c & 0x0F];

   return ss.str();
}

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

const secp256k1_context* _get_context()
{
   static secp256k1_context* ctx = secp256k1_context_create( SECP256K1_CONTEXT_VERIFY | SECP256K1_CONTEXT_SIGN );
   return ctx;
}

void _init_lib()
{
   static const secp256k1_context* ctx = _get_context();
   static int init_o = init_openssl();
   boost::ignore_unused(ctx, init_o);
}

static int extended_nonce_function( unsigned char *nonce32, const unsigned char *msg32,
                                    const unsigned char *key32, const unsigned char* algo16,
                                    void *data, const unsigned int attempt )
{
   unsigned int* extra = (unsigned int*) data;
   (*extra)++;
   return secp256k1_nonce_function_default( nonce32, msg32, key32, algo16, nullptr, *extra );
}

static const public_key_data empty_pub{};
static const private_key_secret empty_priv{};


public_key::public_key() { _init_lib(); }

public_key::public_key( const public_key& pk ) : _key( pk._key ) { _init_lib(); }

public_key::public_key( public_key&& pk ) : _key( std::move( pk._key ) ) { _init_lib(); }

public_key::~public_key() {}

compressed_public_key public_key::serialize()const
{
   KOINOS_ASSERT( _key != empty_pub, key_serialization_error, "Cannot serialize an empty public key", () );

   compressed_public_key cpk;
   size_t len = cpk.data.size();
   KOINOS_ASSERT(
      secp256k1_ec_pubkey_serialize(
         _get_context(),
         (unsigned char *) cpk.data.data(),
         &len,
         (const secp256k1_pubkey*) _key.data.data(),
         SECP256K1_EC_COMPRESSED ),
      key_serialization_error, "Unknown error during public key serialization", () );
   KOINOS_ASSERT( len == cpk.data.size(), key_serialization_error,
      "Serialized key does not match expected size of ${n} bytes", ("n", cpk.data.size()) );

   return cpk;
}

public_key public_key::deserialize( const compressed_public_key& cpk )
{
   public_key pk;
   KOINOS_ASSERT(
      secp256k1_ec_pubkey_parse(
         _get_context(),
         (secp256k1_pubkey*) pk._key.data.data(),
         (const unsigned char*) cpk.data.data(),
         cpk.data.size() ),
      key_serialization_error, "Unknown serror during public key deserialization", () );
   return pk;
}


public_key public_key::recover( const recoverable_signature& sig, const multihash_type& digest )
{
   KOINOS_ASSERT( multihash::get_size( digest ) == 32, key_recovery_error, "Digest must be 32 bytes", () );
   KOINOS_ASSERT( is_canonical( sig ), key_recovery_error, "Signature is not canonical", () );
   fl_blob<65> internal_sig;
   public_key pk;

   int32_t rec_id = sig.data[0];
   KOINOS_ASSERT( 31 <= rec_id && rec_id <= 33, key_recovery_error, "Recovery ID mismatch. Must be in range [31,33]", () );

   // The internal representation, as per the secp256k1 documentation, is an implementation
   // detail and not guaranteed across platforms or versions of secp256k1. We need to
   // convert the portable format to the internal format first.
   KOINOS_ASSERT(
      secp256k1_ecdsa_recoverable_signature_parse_compact(
         _get_context(),
         (secp256k1_ecdsa_recoverable_signature*) internal_sig.data.data(),
         (const unsigned char*) sig.data.data() + 1,
         rec_id >> 5 ),
      key_recovery_error, "Unknown error when parsing signature", () );

   KOINOS_ASSERT(
      secp256k1_ecdsa_recover(
         _get_context(),
         (secp256k1_pubkey*) pk._key.data.data(),
         (const secp256k1_ecdsa_recoverable_signature*) internal_sig.data.data(),
         (unsigned char*) digest.digest.data.data() ),
      key_recovery_error, "Unknown error recovering public key from signature", () );
   return pk;
}

public_key public_key::add( const multihash_type& digest )const
{
   KOINOS_ASSERT( multihash::get_size( digest ), key_manipulation_error, "Digest must be 32 bytes", () );
   KOINOS_ASSERT( _key != empty_pub, key_manipulation_error, "Cannot add to an empty key", () );
   public_key new_key( *this );
   KOINOS_ASSERT(
      secp256k1_ec_pubkey_tweak_add(
         _get_context(),
         (secp256k1_pubkey*) new_key._key.data.data(),
         (unsigned char*) digest.digest.data.data() ),
      key_manipulation_error, "Unknown error when adding to public key", () );
   return new_key;
}

public_key& public_key::operator=( const public_key& pk )
{
   _key = pk._key;
   return *this;
}

public_key& public_key::operator=( public_key&& pk )
{
   _key = std::move( pk._key );
   return *this;
}

bool public_key::valid()const
{
   return _key != empty_pub;
}

unsigned int public_key::fingerprint() const
{
   multihash_type sha256 = hash( CRYPTO_SHA2_256_ID, _key.data.data(), _key.data.size() );
   multihash_type ripemd160 = hash( CRYPTO_RIPEMD160_ID, sha256.digest.data.data(), sha256.digest.data.size() );
   unsigned char* fp = (unsigned char*) ripemd160.digest.data.data();
   return (fp[0] << 24) | (fp[1] << 16) | (fp[2] << 8) | fp[3];
}

bool public_key::is_canonical( const recoverable_signature& c )
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
   KOINOS_ASSERT( _key != empty_pub, key_serialization_error, "Cannot serialize an empty key", () );
   compressed_public_key cpk = serialize();
   return to_base58( cpk );
}

std::string public_key::to_base58( const compressed_public_key &key )
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
   KOINOS_ASSERT( koinos::pack::util::decode_base58( b58, d.data ),
      key_serialization_error, "Base58 string is not the correct size for a 37 byte key", () );
   compressed_public_key key;
   uint32_t check = *((uint32_t*)hash( CRYPTO_SHA2_256_ID, d.data.data(), key.data.size() ).digest.data.data());
   KOINOS_ASSERT( memcmp( (char*)&check, d.data.data() + sizeof(key), sizeof(check) ) == 0,
      key_serialization_error, "Invlaid checksum", () );
   memcpy( (char*)key.data.data(), d.data.data(), sizeof(key) );
   return deserialize( key );
}


private_key::private_key() { _init_lib(); }

private_key::private_key( private_key&& pk ) : _key( std::move( pk._key ) ) { _init_lib(); }

private_key::private_key( const private_key& pk ) : _key( pk._key ) { _init_lib(); }

private_key::~private_key() {}

private_key& private_key::operator=( private_key&& pk )
{
   _key = std::move( pk._key );
   return *this;
}

private_key& private_key::operator=( const private_key& pk )
{
   _key = pk._key;
   return *this;
}

private_key private_key::regenerate( const multihash_type& secret )
{
   multihash::validate_sha256( secret );
   private_key self;
   memcpy( self._key.data.data(), secret.digest.data.data(), self._key.data.size() );
   return self;
}

private_key private_key::generate_from_seed( const multihash_type& seed, const multihash_type& offset )
{
   // There is non-determinism in this function, perhaps purposefully.
   ssl_bignum z;
   BN_bin2bn((unsigned char*)offset.digest.data.data(), offset.digest.data.size(), z);

   ec_group group(EC_GROUP_new_by_curve_name(NID_secp256k1));
   bn_ctx ctx(BN_CTX_new());
   ssl_bignum order;
   EC_GROUP_get_order(group, order, ctx);

   // secexp = (seed + z) % order
   ssl_bignum secexp;
   BN_bin2bn((unsigned char*)&seed, sizeof(seed), secexp);
   BN_add(secexp, secexp, z);
   BN_mod(secexp, secexp, order, ctx);

   multihash_type secret;
   multihash::set_id( secret, CRYPTO_SHA2_256_ID );
   multihash::set_size( secret, 32 );
   secret.digest.data.resize( 32 );
   assert(BN_num_bytes(secexp) <= int64_t(secret.digest.data.size()));
   auto shift = secret.digest.data.size() - BN_num_bytes(secexp);
   BN_bn2bin(secexp, ((unsigned char*)secret.digest.data.data())+shift);
   return regenerate( secret );
}

private_key_secret private_key::get_secret()const
{
   return _key;
}

recoverable_signature private_key::sign_compact( const multihash_type& digest )const
{
   multihash::validate_sha256( digest );
   KOINOS_ASSERT( _key != empty_priv, signing_error, "Cannot sign with an empty key", () );
   fl_blob<65> internal_sig;
   recoverable_signature sig;
   int32_t rec_id;
   unsigned int counter = 0;
   do
   {
      KOINOS_ASSERT(
         secp256k1_ecdsa_sign_recoverable(
            _get_context(),
            (secp256k1_ecdsa_recoverable_signature*) internal_sig.data.data(),
            (unsigned char*) digest.digest.data.data(),
            (unsigned char*) _key.data.data(),
            extended_nonce_function,
            &counter ),
         signing_error, "Unknown error when signing", () );
      KOINOS_ASSERT(
         secp256k1_ecdsa_recoverable_signature_serialize_compact(
            _get_context(),
            (unsigned char*) sig.data.data() + 1,
            &rec_id,
            (const secp256k1_ecdsa_recoverable_signature*) internal_sig.data.data() ),
         signing_error, "Unknown error when serialzing recoverable signature", () );
      sig.data[0] = (char)rec_id + 31;
   } while( !public_key::is_canonical( sig ) );
   return sig;
}

public_key private_key::get_public_key()const
{
   KOINOS_ASSERT( _key != empty_priv, key_manipulation_error, "Cannot get private key of an empty public key", () );
   public_key pk;
   KOINOS_ASSERT(
      secp256k1_ec_pubkey_create(
         _get_context(),
         (secp256k1_pubkey*) pk._key.data.data(),
         (unsigned char*) _key.data.data() ),
      key_manipulation_error, "Unknown error creating public key from a private key", () );
   return pk;
}

} // koinos::crypto
