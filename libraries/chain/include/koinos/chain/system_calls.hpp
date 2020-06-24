#pragma once

#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/types.hpp>

#include <koinos/pack/rt/basetypes.hpp>

#include <koinos/statedb/statedb_types.hpp>

namespace koinos::chain {

using koinos::types::system::system_call_id;
using koinos::types::thunks::thunk_id;

KOINOS_DECLARE_DERIVED_EXCEPTION( unknown_system_call, chain_exception );

// First 160 bits are obtained by 160-bit truncation of sha256("object_space:system_call")
const statedb::object_space SYS_CALL_DISPATCH_TABLE_SPACE_ID = types::uint256_t("0xd15cd01c47057163768c9d339a81495e6d167f20000000000000000000000001");
// 20 bytes contract ID + 1 byte variant
const int64_t SYS_CALL_DISPATCH_TABLE_OBJECT_MAX_SIZE = 21;

std::optional< thunk_id > get_default_system_call_entry( system_call_id sid );

} // koinos::chain
