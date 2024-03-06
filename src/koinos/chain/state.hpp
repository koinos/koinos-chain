#pragma once

#include <algorithm>
#include <cstdint>

#include <koinos/chain/chain.pb.h>
#include <koinos/chain/execution_context.hpp>

#include <koinos/bigint.hpp>
#include <koinos/common.pb.h>
#include <koinos/crypto/multihash.hpp>
#include <koinos/state_db/state_db_types.hpp>
#include <koinos/util/conversion.hpp>

namespace koinos::chain { namespace state {

namespace zone {

const auto kernel = std::string{};

} // namespace zone

namespace space {

const object_space contract_bytecode();
const object_space contract_metadata();
const object_space system_call_dispatch();
const object_space metadata();
const object_space transaction_nonce();

} // namespace space

namespace key {

const auto head_block = util::converter::as< std::string >(
  crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::head_block" ) ) );
const auto chain_id = util::converter::as< std::string >(
  crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::chain_id" ) ) );
const auto genesis_key = util::converter::as< std::string >(
  crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::genesis_key" ) ) );
const auto resource_limit_data = util::converter::as< std::string >(
  crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::resource_limit_data" ) ) );
const auto max_account_resources = util::converter::as< std::string >(
  crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::max_account_resources" ) ) );
const auto protocol_descriptor = util::converter::as< std::string >(
  crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::protocol_descriptor" ) ) );
const auto compute_bandwidth_registry = util::converter::as< std::string >(
  crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::compute_bandwidth_registry" ) ) );
const auto block_hash_code = util::converter::as< std::string >(
  crypto::hash( crypto::multicodec::sha2_256, std::string( "object_key::block_hash_code" ) ) );

} // namespace key

namespace system_call_dispatch {

// Size for buffer when fetching system call from database -> 1 for variant, 20 for contract_id, 4 for entry_point
constexpr uint32_t max_object_size = 512;

} // namespace system_call_dispatch

constexpr uint32_t max_object_size = 1'024 * 1'024; // 1 MB

void assert_permissions( execution_context& context, const object_space& space );

}} // namespace koinos::chain::state
