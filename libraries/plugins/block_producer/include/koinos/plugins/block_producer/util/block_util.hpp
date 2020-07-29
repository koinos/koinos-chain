#pragma once

#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/pack/classes.hpp>

namespace koinos::plugins::block_producer::util {

//
// +-----------+      +--------------+      +-------------------------+      +---------------------+
// | Block sig | ---> | Block active | ---> | Transaction merkle root | ---> | Transaction actives |
// +-----------+      +--------------+      +-------------------------+      +---------------------+
//                           |
//                           V
//                +----------------------+      +----------------------+
//                |                      | ---> |     Block passive    |
//                |                      |      +----------------------+
//                |                      |
//                |                      |      +----------------------+
//                | Passives merkle root | ---> | Transaction passives |
//                |                      |      +----------------------+
//                |                      |
//                |                      |      +----------------------+
//                |                      | ---> |   Transaction sigs   |
//                +----------------------+      +----------------------+
//

void set_block_merkle_roots( types::protocol::block& block, uint64_t code = CRYPTO_SHA2_256_ID, uint64_t size = 0 )
{
   std::vector< types::multihash > trx_active_hashes( block.transactions.size() );
   std::vector< types::multihash > passive_hashes( block.transactions.size() + 1 );

   // Hash transaction actives, passives, and signatures for merkle roots
   for ( size_t i = 0; i < block.transactions.size(); i++ )
   {
      trx_active_hashes[i]      = crypto::hash( code, block.transactions[i]->active_data, size );
      passive_hashes[2*(i+1)]   = crypto::hash( code, block.transactions[i]->passive_data, size );
      passive_hashes[2*(i+1)+1] = crypto::hash_blob( code, block.transactions[i]->signature_data, size );
   }

   passive_hashes[0] = crypto::hash( code, block.passive_data, size );
   passive_hashes[0] = crypto::empty_hash( code, size );

   crypto::merkle_hash_leaves( trx_active_hashes, code, size );
   crypto::merkle_hash_leaves( passive_hashes,    code, size );

   block.active_data->header_hashes.digests.resize(3);
   block.active_data->header_hashes.digests[(uint32_t)types::protocol::header_hash_index::transaction_merkle_root_hash_index] = trx_active_hashes[0].digest;
   block.active_data->header_hashes.digests[(uint32_t)types::protocol::header_hash_index::passive_data_merkle_root_hash_index] = passive_hashes[0].digest;
   block.active_data->header_hashes.id = code;
}

void sign_block( types::protocol::block& block, crypto::private_key& block_signing_key )
{
   // Signature is on the hash of the active data
   types::multihash digest = crypto::hash( CRYPTO_SHA2_256_ID, block.active_data );
   auto signature = block_signing_key.sign_compact( digest );
   pack::to_variable_blob( block.signature_data, signature );
}

} // koinos::plugins::block_producer::util
