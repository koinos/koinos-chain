#pragma once

#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/types.hpp>

#include <koinos/pack/system_call_ids.hpp>
#include <koinos/pack/thunk_ids.hpp>

#include <koinos/statedb/statedb_types.hpp>

namespace koinos::chain {

using koinos::types::system::system_call_id;
using koinos::types::thunks::thunk_id;

KOINOS_DECLARE_DERIVED_EXCEPTION( unknown_system_call, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( invalid_contract, chain_exception );

// First 160 bits are obtained by 160-bit truncation of sha256("object_space::contract")
const statedb::object_space CONTRACT_SPACE_ID = types::uint256_t( "0x3e5bb9473a9187e1be1c8321fd2a44b9b85510a0000000000000000000000001" );
// First 160 bits are obtained by 160-bit truncation of sha256("object_space::system_call")
const statedb::object_space SYS_CALL_DISPATCH_TABLE_SPACE_ID = types::uint256_t( "0xd15cd01c47057163768c9d339a81495e6d167f20000000000000000000000001" );
// Size for buffer when fetching system call from database -> 1 for variant, 20 for contract_id, 4 for entry_point
const int64_t SYS_CALL_DISPATCH_TABLE_OBJECT_MAX_SIZE = 1 + 20 + 4;

std::optional< thunk_id > get_default_system_call_entry( system_call_id sid );

} // koinos::chain
