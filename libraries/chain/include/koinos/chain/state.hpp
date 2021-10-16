#pragma once

#include <algorithm>
#include <cstdint>

#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/chain.pb.h>

#include <koinos/bigint.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/common.pb.h>
#include <koinos/conversion.hpp>
#include <koinos/state_db/state_db_types.hpp>
#include <koinos/util.hpp>

namespace koinos::chain::state {

namespace zone {

const auto kernel = std::string{};

} // zone

enum class id : uint32_t
{
   meta,
   system_call_dispatch,
   contract
};

namespace space {

const object_space contract();
const object_space system_call_dispatch();
const object_space meta();

} // space

namespace key {

const auto head_block_time   = converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::head_block_time" ) ) );
const auto chain_id          = converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::chain_id" ) ) );

std::string transaction_nonce( const std::string& payer );

} // key

namespace system_call_dispatch {

// Size for buffer when fetching system call from database -> 1 for variant, 20 for contract_id, 4 for entry_point
constexpr uint32_t max_object_size = 512;

} // system_call_dispatch

constexpr uint32_t max_object_size = 1024 * 1024; // 1 MB

void assert_permissions( const apply_context& context, const object_space& space );

void assert_transaction_nonce( apply_context& ctx, const std::string& payer, uint64_t nonce );
void update_transaction_nonce( apply_context& ctx, const std::string& payer, uint64_t nonce );

} // koinos::chain::state
