
#pragma once
#include <koinos/statedb/statedb_types.hpp>

#include <boost/any.hpp>
#include <boost/filesystem.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include <memory>
#include <vector>

#define STATE_DB_MAX_OBJECT_SIZE 208896

namespace koinos { namespace statedb {

namespace detail {
class state_db_impl;
class state_node_impl;
}

struct get_object_args
{
   object_space    space;
   object_key      key;
   char*           buf = nullptr;
   uint64_t        buf_size = 0;
};

struct get_object_result
{
   object_key      key;
   int64_t         size = 0;
};

struct put_object_args
{
   object_space    space;
   object_key      key;
   char*           buf = nullptr;    // null -> delete object
   uint64_t        object_size = 0;
};

struct put_object_result
{
   bool            object_existed = false;
};

/**
 * Allows querying the database at a particular checkpoint.
 */
class state_node final
{
   public:
      state_node();
      ~state_node();

      /**
       * Fetch an object if one exists.
       *
       * - Size of the object is written into result.size
       * - Object's value is copied into args.buf, provided buf != nullptr and buf_size is large enough
       * - If buf is too small, buf is unchanged, however result is still updated
       * - args.key is copied into result.key
       */
      void get_object( get_object_result& result, const get_object_args& args );

      /**
       * Get the next object.
       *
       * - Size of the object is written into result.size
       * - Object's value is copied into args.buf, provided buf != nullptr and buf_size is large enough
       * - If buf is too small, buf is unchanged, however result is still updated
       * - Found key is written into result
       */
      void get_next_object( get_object_result& result, const get_object_args& args );

      /**
       * Get the previous object.
       *
       * - Size of the object is written into result.size
       * - Object's value is copied into args.buf, provided buf != nullptr and buf_size is large enough
       * - If buf is too small, buf is unchanged, however result is still updated
       * - Found key is written into result
       */
      void get_prev_object( get_object_result& result, const get_object_args& args );

      /**
       * Write an object into the state_node.
       *
       * - Fail if node is not writable.
       * - If object exists, object is overwritten.
       * - If buf == nullptr, object is deleted.
       */
      void put_object( put_object_result& result, const put_object_args& args );

      /**
       * Return true if the node is writable.
       */
      bool is_writable();

      state_node_id node_id();
      uint64_t      revision();

      friend class detail::state_db_impl;

   private:
      std::unique_ptr< detail::state_node_impl > impl;
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
class state_db final
{
   public:
      state_db();
      ~state_db();

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
      std::shared_ptr< state_node > get_empty_node();

      /**
       * Get the state node ID of some recent state nodes.
       *
       * This method is useful for finding state in an existing database.
       */
      void get_recent_states(std::vector<state_node_id>& node_id_list, int limit);

      /**
       * Get the state_node for the given state_node_id.
       */
      std::shared_ptr< state_node > get_node( state_node_id node_id );

      /**
       * Create a writable state_node.
       *
       * - If parent_id refers to a writable node, fail.
       * - Otherwise, return a new writable node.
       * - Writing to the returned node will not modify the parent node.
       *
       * If the parent is subsequently discarded, state_db preserves
       * as much of the parent's state storage as necessary to continue
       * to serve queries on any (non-discarded) children.  A discarded
       * parent node's state may internally be merged into a child's
       * state storage area, allowing the parent's state storage area
       * to be freed.  This merge may occur immediately, or it may be
       * deferred or parallelized.
       */
      std::shared_ptr< state_node > create_writable_node( state_node_id parent_id, state_node_id new_id );

      /**
       * Finalize a node.  The node will no longer be writable.
       */
      void finalize_node( state_node_id node_id );

      /**
       * Discard the node, it can no longer be used.
       */
      void discard_node( state_node_id node_id );

      /**
       * Squash the node in to the root state, committing it.
       * Branching state between this node and its ancestor will be discarded.
       */
      void commit_node( state_node_id node_id );

      /**
       * Get and return the current "head" node.
       *
       * Head is determined by longest chain. Oldest
       * chain wins in a tie of length.
       */
      std::shared_ptr< state_node > get_head();

   private:
      std::unique_ptr< detail::state_db_impl > impl;
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
