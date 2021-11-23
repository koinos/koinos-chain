#pragma once

#include <algorithm>
#include <cstdint>

#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/chain.pb.h>

#include <koinos/bigint.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/common.pb.h>
#include <koinos/state_db/state_db_types.hpp>
#include <koinos/util/conversion.hpp>

namespace koinos::chain {

bool operator<( const object_space& lhs, const object_space& rhs );

namespace state {

namespace zone {

const auto kernel = std::string{};

} // zone

enum class id : uint32_t
{
   meta,
   system_call_dispatch,
   contract_bytecode,
   contract_hash,
   transaction_nonce
};

namespace space {

const object_space contract_bytecode();
const object_space contract_hash();
const object_space system_call_dispatch();
const object_space meta();
const object_space transaction_nonce();

} // space

namespace key {

const auto head_block_time = util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::head_block_time" ) ) );
const auto chain_id        = util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::chain_id" ) ) );

} // key

namespace system_call_dispatch {

// Size for buffer when fetching system call from database -> 1 for variant, 20 for contract_id, 4 for entry_point
constexpr uint32_t max_object_size = 512;

} // system_call_dispatch

constexpr uint32_t max_object_size = 1024 * 1024; // 1 MB

void assert_permissions( const execution_context& context, const object_space& space );

} // state

} // koinos::chain
