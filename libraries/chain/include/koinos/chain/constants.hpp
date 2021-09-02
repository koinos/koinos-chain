#pragma once

#include <algorithm>
#include <cstdint>

#include <koinos/common.pb.h>
#include <koinos/statedb/statedb_types.hpp>
#include <koinos/util.hpp>

namespace koinos::chain {

namespace database {

namespace detail {

std::array< std::byte, 256 > convert( crypto::multihash&& m )
{
   auto v = m.as< std::vector< std::byte > >();

   std::array< std::byte, 256 > arr;
   std::copy_n( std::make_move_iterator( v.begin() ), 256, arr.begin() );

   return arr;
}

} // detail

namespace space {

const statedb::object_space contract             = detail::convert( crypto::hash( crypto::multicodec::sha2_256, std::string( "object_space::contract" ) ) );
const statedb::object_space system_call_dispatch = detail::convert( crypto::hash( crypto::multicodec::sha2_256, std::string( "object_space::system_call" ) ) );
const statedb::object_space kernel               = detail::convert( crypto::hash( crypto::multicodec::sha2_256, std::string( "object_space::kernel" ) ) );

} // space

// Size for buffer when fetching system call from database -> 1 for variant, 20 for contract_id, 4 for entry_point
const int64_t system_call_dispatch_object_max_size = 1 + 20 + 4;

// inline statedb::object_key key_from_string( const std::string& s )
// {
//    statedb::object_key key;
//    assert( s.size() <= key.size() );
//    std::copy( s.begin(), s.end(), key.data() );
//    return key;
// }

namespace key {

const statedb::object_key transaction_nonce = detail::convert( crypto::hash( crypto::multicodec::sha2_256, std::string( "nonce" ) ) );
const statedb::object_key head_block_time   = detail::convert( crypto::hash( crypto::multicodec::sha2_256, std::string( "head_block_time" ) ) );
const statedb::object_key chain_id          = detail::convert( crypto::hash( crypto::multicodec::sha2_256, std::string( "chain_id" ) ) );

} // key

} // database

// Default irreversible block threshold
const uint64_t default_irreversible_threshold = 60;

} // koinos::chain
