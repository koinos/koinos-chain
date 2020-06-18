#pragma once

#include <koinos/exception.hpp>
#include <koinos/pack/rt/basetypes.hpp>

#include <koinos/statedb/statedb_types.hpp>

namespace koinos::chain {

DECLARE_KOINOS_EXCEPTION( unknown_system_call );

using koinos::protocol::vl_blob;

// First 160 bits are obtained by 160-bit truncation of sha256("object_space:xcall")
const statedb::object_space SYS_CALL_DISPATCH_TABLE_SPACE_ID = protocol::uint256_t("0xd34ee9149a24c052cfad6fefea6b6d2b2b08a840000000000000000000000001");
// 20 bytes contract ID + 1 byte variant
const int64_t SYS_CALL_DISPATCH_TABLE_OBJECT_MAX_SIZE = 21;

vl_blob get_default_sys_call_entry( uint32_t sid );

} // koinos::chain
