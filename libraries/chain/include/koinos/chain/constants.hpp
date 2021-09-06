#pragma once

#include <algorithm>
#include <cstdint>

#include <koinos/bigint.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/common.pb.h>
#include <koinos/conversion.hpp>
#include <koinos/statedb/statedb_types.hpp>
#include <koinos/util.hpp>

namespace koinos::chain {

namespace database {

namespace space {

const statedb::object_space contract             = converter::as< statedb::object_space >( crypto::hash( crypto::multicodec::ripemd_160, std::string( "object_space::contract" ) ) );
const statedb::object_space system_call_dispatch = converter::as< statedb::object_space >( crypto::hash( crypto::multicodec::ripemd_160, std::string( "object_space::system_call" ) ) );
const statedb::object_space kernel               = converter::as< statedb::object_space >( crypto::hash( crypto::multicodec::ripemd_160, std::string( "object_space::kernel" ) ) );

} // space

namespace key {

const statedb::object_key head_block_time   = converter::as< statedb::object_key >( crypto::hash( crypto::multicodec::ripemd_160, std::string( "object_key::head_block_time" ) ) );
const statedb::object_key chain_id          = converter::as< statedb::object_key >( crypto::hash( crypto::multicodec::ripemd_160, std::string( "object_key::chain_id" ) ) );

inline statedb::object_key transaction_nonce( const std::string& payer )
{
   auto payer_key = converter::to< uint160_t >( payer );
   auto trx_nonce_key = converter::to< uint64_t >( crypto::hash( crypto::multicodec::ripemd_160, std::string( "object_key::nonce" ) ).digest() );
   return converter::as< statedb::object_key >( payer_key, trx_nonce_key );
}

} // key

namespace system_call_dispatch {

// Size for buffer when fetching system call from database -> 1 for variant, 20 for contract_id, 4 for entry_point
const int64_t max_object_size = 1 + 20 + 4;

} // system_call_dispatch

constexpr std::size_t max_object_size = 1024 * 1024; // 1 MB

} // database

// Default irreversible block threshold
const uint64_t default_irreversible_threshold = 60;

} // koinos::chain
