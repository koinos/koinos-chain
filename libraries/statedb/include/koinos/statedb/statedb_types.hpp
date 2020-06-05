#pragma once
#include <koinos/exception.hpp>

#include <koinos/pack/rt/basetypes.hpp>

#include <boost/multiprecision/cpp_int.hpp>

namespace koinos::statedb {

DECLARE_KOINOS_EXCEPTION( database_not_open );

/**
 * An attempt was made to modify a finalized node.
 */
DECLARE_KOINOS_EXCEPTION( node_finalized );

/**
 * An argument is out of range or otherwise invalid.
 *
 * If IllegalArgument is thrown, it likely indicates a programming error
 * in the caller.
 */
DECLARE_KOINOS_EXCEPTION( illegal_argument );

/**
 * The given NodeId cannot be discarded.
 *
 * Due to limitations of chainbase, the only sessions that can be discarded are:
 * - The oldest session.
 * - The session before the newest session.
 *
 * Furthermore the last node cannot be discarded.
 */
DECLARE_KOINOS_EXCEPTION( cannot_discard );

/**
 * An internal invariant has been violated.
 *
 * This is most likely caused by a programming error in state_db.
 */
DECLARE_KOINOS_EXCEPTION( internal_error );

// object_space / object_key don't actually use any of the cryptography features of fc::sha256
// They just use sha256 as an FC serializable 256-bit integer type

using boost::multiprecision::uint256_t;

typedef protocol::multihash_type   state_node_id;
typedef uint256_t                  object_space;
typedef uint256_t                  object_key;
typedef protocol::vl_blob          object_value;

} // koinos::statedb
