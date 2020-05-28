#pragma once
#include <koinos/exception.hpp>

#include <koinos/crypto/multihash.hpp>

#include <boost/multiprecision/cpp_int.hpp>

namespace koinos::statedb {

DECLARE_KOINOS_EXCEPTION( database_not_open );

/**
 * The caller attempts to maintain live references to multiple nodes.
 *
 * Due to limitations of chainbase backing, the current implementation
 * only allows one state_node to exist at a time.  The caller must discard
 * its current state_node before calling a method that could create a new
 * state_node.
 */
DECLARE_KOINOS_EXCEPTION( node_not_expired );

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
 * No node with the given NodeId exists.
 */
DECLARE_KOINOS_EXCEPTION( unknown_node );

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
 * The given tree manipulation cannot be performed due to node position.
 *
 * Due to the limitations of chainbase, only certain nodes may be discarded,
 * read, or written.
 */
DECLARE_KOINOS_EXCEPTION( bad_node_position );

/**
 * An internal invariant has been violated.
 *
 * This is most likely caused by a programming error in state_db.
 */
DECLARE_KOINOS_EXCEPTION( internal_error );

// object_space / object_key don't actually use any of the cryptography features of fc::sha256
// They just use sha256 as an FC serializable 256-bit integer type

using boost::multiprecision::uint256_t;

typedef uint256_t                  state_node_id;
typedef uint256_t                  object_space;
typedef uint256_t                  object_key;
typedef std::string                object_value;

inline uint256_t u256_from_mh( koinos::protocol::multihash_type& mh )
{
   assert( sizeof( uint256_t ) == mh.digest.data.size() );
   uint256_t res;
   memcpy( (char*)&res, mh.digest.data.data(), mh.digest.data.size() );
   return res;
}

inline koinos::protocol::multihash_type mh_from_u256( uint256_t u256 )
{
   koinos::protocol::multihash_type mh;
   mh.digest.data.resize( sizeof( uint256_t ) );
   memcpy( mh.digest.data.data(), (char*)&u256, sizeof( uint256_t ) );
   koinos::crypto::multihash::set_id( mh, CRYPTO_SHA2_256_ID );
   koinos::crypto::multihash::set_size( mh, sizeof( uint256_t ) );
   return mh;
}

} // koinos::statedb
