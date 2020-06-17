
namespace koinos { namespace protocol {

struct unused_extensions_type { };

// KOINOS_SIGNATURE_LENGTH = 65 from fc::ecc::compact_signature
typedef fixed_blob<65> signature_type;
// CONTRACT_ID_LENGTH = 20 from ripemd160 = 160 bits
typedef fixed_blob<20> contract_id_type;

} }
