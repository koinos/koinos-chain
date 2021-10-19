#pragma once

#include <algorithm>
#include <cstdint>

#include <koinos/bigint.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/common.pb.h>
#include <koinos/state_db/state_db_types.hpp>
#include <koinos/util/conversion.hpp>

namespace koinos::chain {

namespace database {

namespace space {

const auto contract             = util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, std::string( "object_space::contract" ) ) );
const auto system_call_dispatch = util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, std::string( "object_space::system_call" ) ) );
const auto kernel               = util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, std::string( "object_space::kernel" ) ) );

} // space

namespace key {

const auto head_block_time   = util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::head_block_time" ) ) );
const auto chain_id          = util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::chain_id" ) ) );

inline std::string transaction_nonce( const std::string& payer )
{
   auto payer_key = util::converter::to< uint160_t >( payer );
   auto trx_nonce_key = util::converter::to< uint64_t >( crypto::hash( crypto::multicodec::ripemd_160, std::string( "object_key::nonce" ) ).digest() );
   return util::converter::as< std::string >( payer_key, trx_nonce_key );
}

} // key

namespace system_call_dispatch {

// Size for buffer when fetching system call from database -> 1 for variant, 20 for contract_id, 4 for entry_point
constexpr uint32_t max_object_size = 512;

} // system_call_dispatch

constexpr uint32_t max_object_size = 1024 * 1024; // 1 MB

} // database

// Default irreversible block threshold
constexpr uint64_t default_irreversible_threshold = 60;

} // koinos::chain
