#pragma once

#include <cstdint>

#include <koinos/statedb/statedb_types.hpp>

namespace koinos::chain {

// First 160 bits are obtained by 160-bit truncation of sha256("object_space::contract")
const statedb::object_space CONTRACT_SPACE_ID = uint256( "0x3e5bb9473a9187e1be1c8321fd2a44b9b85510a0000000000000000000000001" );

// First 160 bits are obtained by 160-bit truncation of sha256("object_space::system_call")
const statedb::object_space SYS_CALL_DISPATCH_TABLE_SPACE_ID = uint256( "0xd15cd01c47057163768c9d339a81495e6d167f20000000000000000000000001" );

// First 160 bits are obtained by 160-bit truncation of sha256("object_space::kernel")
const statedb::object_space KERNEL_SPACE_ID = uint256( "0xc72196cc927d29fc4c456f8672fc5d5141399be0000000000000000000000001" );

// Size for buffer when fetching system call from database -> 1 for variant, 20 for contract_id, 4 for entry_point
const int64_t SYS_CALL_DISPATCH_TABLE_OBJECT_MAX_SIZE = 1 + 20 + 4;

} // koinos::chain

