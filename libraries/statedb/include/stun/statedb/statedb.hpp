
#pragma once

#include <koinos/exception.hpp>

#include <boost/any.hpp>
#include <boost/filesystem.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include <memory>
#include <vector>

#define STATEDB_MAX_OBJECT_SIZE 208896

namespace koinos { namespace statedb {

DECLARE_KOINOS_EXCEPTION( DatabaseNotOpen );
/**
 * The caller attempts to maintain live references to multiple nodes.
 *
 * Due to limitations of chainbase backing, the current implementation
 * only allows one StateNode to exist at a time.  The caller must discard
 * its current StateNode before calling a method that could create a new
 * StateNode.
 */
DECLARE_KOINOS_EXCEPTION( NodeNotExpired );
/**
 * An argument is out of range or otherwise invalid.
 *
 * If IllegalArgument is thrown, it likely indicates a programming error
 * in the caller.
 */
DECLARE_KOINOS_EXCEPTION( IllegalArgument );

/**
 * No node with the given NodeId exists.
 */
DECLARE_KOINOS_EXCEPTION( UnknownNode );

/**
 * The given NodeId cannot be discarded.
 *
 * Due to limitations of chainbase, the only sessions that can be discarded are:
 * - The oldest session.
 * - The session before the newest session.
 *
 * Furthermore the last node cannot be discarded.
 */
DECLARE_KOINOS_EXCEPTION( CannotDiscard );

/**
 * The given tree manipulation cannot be performed due to node position.
 *
 * Due to the limitations of chainbase, only certain nodes may be discarded,
 * read, or written.
 */
DECLARE_KOINOS_EXCEPTION( BadNodePosition );

/**
 * An internal invariant has been violated.
 *
 * This is most likely caused by a programming error in StateDB.
 */
DECLARE_KOINOS_EXCEPTION( InternalError );

namespace detail {
class StateDB_Impl;
class StateNode_Impl;
}

// ObjectSpace / ObjectKey don't actually use any of the cryptography features of fc::sha256
// They just use sha256 as an FC serializable 256-bit integer type

using boost::multiprecision::uint256_t;

typedef int64_t                    StateNodeId;
typedef uint256_t                  ObjectSpace;
typedef uint256_t                  ObjectKey;
typedef std::string                ObjectValue;

struct GetObjectArgs
{
   ObjectSpace     space;
   ObjectKey       key;
   char*           buf = nullptr;
   uint64_t        buf_size = 0;
};

struct GetObjectResult
{
   ObjectKey       key;
   int64_t         size = 0;
};

struct PutObjectArgs
{
   ObjectSpace     space;
   ObjectKey       key;
   char*           buf = nullptr;    // null -> delete object
   uint64_t        object_size = 0;
};

struct PutObjectResult
{
   bool            object_existed = false;
};

/**
 * Allows querying the database at a particular checkpoint.
 */
class StateNode final
{
   public:
      StateNode();
      ~StateNode();

      /**
       * Fetch an object if one exists.
       *
       * - Size of the object is written into result.size
       * - Object's value is copied into args.buf, provided buf != nullptr and buf_size is large enough
       * - If buf is too small, buf is unchanged, however result is still updated
       * - args.key is copied into result.key
       */
      void get_object( GetObjectResult& result, const GetObjectArgs& args );

      /**
       * Get the next object.
       *
       * - Size of the object is written into result.size
       * - Object's value is copied into args.buf, provided buf != nullptr and buf_size is large enough
       * - If buf is too small, buf is unchanged, however result is still updated
       * - Found key is written into result
       */
      void get_next_object( GetObjectResult& result, const GetObjectArgs& args );

      /**
       * Get the previous object.
       *
       * - Size of the object is written into result.size
       * - Object's value is copied into args.buf, provided buf != nullptr and buf_size is large enough
       * - If buf is too small, buf is unchanged, however result is still updated
       * - Found key is written into result
       */
      void get_prev_object( GetObjectResult& result, const GetObjectArgs& args );

      /**
       * Write an object into the StateNode.
       *
       * - Fail if node is not writable.
       * - If object exists, object is overwritten.
       * - If buf == nullptr, object is deleted.
       */
      void put_object( PutObjectResult& result, const PutObjectArgs& args );

      /**
       * Return true if the node is writable.
       */
      bool is_writable();

      StateNodeId node_id();

      friend class detail::StateDB_Impl;

   private:
      std::unique_ptr< detail::StateNode_Impl > impl;
};

/**
 * Database interface with discardable checkpoints.
 *
 * Currently this is backed by Chainbase, so there are some heavy restrictions on usage:
 *
 * - Checkpoints form a queue internally.
 * - Can discard the second-most-recent checkpoint, this is squash().
 * - Can discard the most-recent checkpoint and revert to the previous checkpoint, this is undo().
 * - Can discard the oldest checkpoint, this is commit().
 *
 * The caller (bcfork) should be written to obey these restrictions.
 * Replacing chainbase with a less restrictive backing store will allow many caller optimizations.
 */
class StateDB final
{
   public:
      StateDB();
      ~StateDB();

      /**
       * Open the database.
       */
      void open( const boost::filesystem::path& p, const boost::any& o );

      /**
       * Close the database.
       */
      void close();

      /**
       * Get the state node representing the empty state.
       *
       * WARNING:  The chainbase implementation of this method will wipe() the database!
       */
      std::shared_ptr< StateNode > get_empty_node();

      /**
       * Get the state node ID of some recent state nodes.
       *
       * This method is useful for finding state in an existing database.
       */
      void get_recent_states(std::vector<StateNodeId>& node_id_list, int limit);

      /**
       * Get the StateNode for the given StateNodeId.
       */
      std::shared_ptr< StateNode > get_node( StateNodeId node_id );

      /**
       * Create a writable StateNode.
       *
       * - If parent_id refers to a writable node, fail.
       * - Otherwise, return a new writable node.
       * - Writing to the returned node will not modify the parent node.
       *
       * If the parent is subsequently discarded, StateDB preserves
       * as much of the parent's state storage as necessary to continue
       * to serve queries on any (non-discarded) children.  A discarded
       * parent node's state may internally be merged into a child's
       * state storage area, allowing the parent's state storage area
       * to be freed.  This merge may occur immediately, or it may be
       * deferred or parallelized.
       */
      std::shared_ptr< StateNode > create_writable_node( StateNodeId parent_id );

      /**
       * Finalize a node.  The node will no longer be writable.
       */
      void finalize_node( StateNodeId node_id );

      /**
       * Discard the node, it can no longer be used.
       */
      void discard_node( StateNodeId node_id );

   private:
      std::unique_ptr< detail::StateDB_Impl > impl;
};


// contract_id   : 160 bits
// reserved      :  72 bits
// object_type   :  24 bits
//
// object_id     : 256 bits

// contract_id is the address of a particular smart contract.
// reserved must be 0
// object_type is semantics defined by the application, different object_type can have different index_type
// object_id is 256 bits, semantics defined by application

} }
