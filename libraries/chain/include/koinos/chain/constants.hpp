#pragma once

#include <algorithm>
#include <cstdint>

#include <koinos/common.pb.h>
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

} // key

} // database

// Size for buffer when fetching system call from database -> 1 for variant, 20 for contract_id, 4 for entry_point
const int64_t system_call_dispatch_object_max_size = 1 + 20 + 4;

// Default irreversible block threshold
const uint64_t default_irreversible_threshold = 60;

} // koinos::chain
