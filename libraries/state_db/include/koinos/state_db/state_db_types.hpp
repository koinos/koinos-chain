#pragma once

#include <cstddef>

#include <koinos/exception.hpp>
#include <koinos/crypto/multihash.hpp>

namespace koinos::state_db {

using state_node_id = crypto::multihash;
using object_space  = std::array< std::byte, 32 >;
using object_key    = std::array< std::byte, 32 >;
using object_value  = std::vector< std::byte >;

KOINOS_DECLARE_EXCEPTION( state_db_exception );

KOINOS_DECLARE_DERIVED_EXCEPTION( database_not_open, state_db_exception );

/**
 * An attempt was made to modify a finalized node.
 */
KOINOS_DECLARE_DERIVED_EXCEPTION( node_finalized, state_db_exception );

/**
 * An argument is out of range or otherwise invalid.
 *
 * If IllegalArgument is thrown, it likely indicates a programming error
 * in the caller.
 */
KOINOS_DECLARE_DERIVED_EXCEPTION( illegal_argument, state_db_exception );

/**
 * The given NodeId cannot be discarded.
 *
 * Due to limitations of chainbase, the only sessions that can be discarded are:
 * - The oldest session.
 * - The session before the newest session.
 *
 * Furthermore the last node cannot be discarded.
 */
KOINOS_DECLARE_DERIVED_EXCEPTION( cannot_discard, state_db_exception );

/**
 * An internal invariant has been violated.
 *
 * This is most likely caused by a programming error in state_db.
 */
KOINOS_DECLARE_DERIVED_EXCEPTION( internal_error, state_db_exception );

} // koinos::state_db
