#pragma once

#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/pack/classes.hpp>

namespace koinos::plugins::block_producer::util {

void set_block_merkle_roots( types::protocol::block& block, uint64_t code = CRYPTO_SHA2_256_ID, uint64_t size = 0 );
void sign_block( types::protocol::block& block, crypto::private_key& block_signing_key );

} // koinos::plugins::block_producer::util
