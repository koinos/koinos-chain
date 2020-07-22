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
   std::vector< types::multihash_type > trx_active_hashes( block.transactions.size() );
   std::vector< types::multihash_type > passive_hashes( block.transactions.size() + 1 );

   // Hash transaction actives, passives, and signatures for merkle roots
   for( size_t i = 0; i < block.transactions.size(); i++ )
   {
      crypto::hash_blob( trx_active_hashes[i],      code, block.transactions[i]->active_data,    size );
      crypto::hash_blob( passive_hashes[2*(i+1)],   code, block.transactions[i]->passive_data,   size );
      crypto::hash_blob( passive_hashes[2*(i+1)+1], code, block.transactions[i]->signature_data, size );
   }

   crypto::hash_blob(  passive_hashes[0], code, block.passive_data, size );
   crypto::empty_hash( passive_hashes[0], code, size );

   crypto::merkle_hash_leaves( trx_active_hashes, code, size );
   crypto::merkle_hash_leaves( passive_hashes,    code, size );

   block.active_data->header_hashes.digests.resize(3);
   block.active_data->header_hashes.digests[(uint32_t)types::protocol::header_hash_index::transaction_merkle_root_hash_index] = trx_active_hashes[0].digest;
   block.active_data->header_hashes.digests[(uint32_t)types::protocol::header_hash_index::passive_data_merkle_root_hash_index] = passive_hashes[0].digest;
   crypto::multihash::set_id( block.active_data->header_hashes, code );
   crypto::multihash::set_size( block.active_data->header_hashes, block.active_data->header_hashes.digests[0].size() );
}

void sign_block( types::protocol::block& block, crypto::private_key& block_signing_key )
{
   // Signature is on the hash of the active data
   crypto::multihash_type digest;
   crypto::hash_blob( digest, CRYPTO_SHA2_256_ID, block.active_data );
   auto signature = block_signing_key.sign_compact( digest );
   pack::to_variable_blob( block.signature_data, signature );
}

} // koinos::plugins::block_producer::util
