#pragma once

#include <cstdint>
#include <koinos/pack/classes.hpp>
#include <koinos/statedb/statedb_types.hpp>

namespace koinos::chain {

namespace database {

// First 160 bits are obtained by 160-bit truncation of sha256("object_space::contract")
const statedb::object_space contract_space = statedb::object_space( "0x3e5bb9473a9187e1be1c8321fd2a44b9b85510a0000000000000000000000001" );

// First 160 bits are obtained by 160-bit truncation of sha256("object_space::system_call")
const statedb::object_space system_call_dispatch_space = statedb::object_space( "0xd15cd01c47057163768c9d339a81495e6d167f20000000000000000000000001" );

// First 160 bits are obtained by 160-bit truncation of sha256("object_space::kernel")
const statedb::object_space kernel_space = statedb::object_space( "0xc72196cc927d29fc4c456f8672fc5d5141399be0000000000000000000000001" );

// Size for buffer when fetching system call from database -> 1 for variant, 20 for contract_id, 4 for entry_point
const int64_t system_call_dispatch_object_max_size = 1 + 20 + 4;

inline statedb::object_key key_from_string( const std::string& s )
{
   auto vkey = pack::to_variable_blob( s );
   vkey.resize( 32, char(0) );
   return pack::from_variable_blob< statedb::object_key >( vkey );
}

namespace key {

const std::string transaction_nonce = "nonce";
const std::string head_block_time   = "head_block_time";
const std::string chain_id          = "chain_id";

} // key

} // database

// Default irreversible block threshold
const block_height_type default_irreversible_threshold = block_height_type{ 60 };

} // koinos::chain
