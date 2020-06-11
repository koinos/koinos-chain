
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
   const char      *buf = nullptr;    // null -> delete object
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
      void get_object( get_object_result& result, const get_object_args& args )const;

      /**
       * Get the next object.
       *
       * - Size of the object is written into result.size
       * - Object's value is copied into args.buf, provided buf != nullptr and buf_size is large enough
       * - If buf is too small, buf is unchanged, however result is still updated
       * - Found key is written into result
       */
      void get_next_object( get_object_result& result, const get_object_args& args )const;

      /**
       * Get the previous object.
       *
       * - Size of the object is written into result.size
       * - Object's value is copied into args.buf, provided buf != nullptr and buf_size is large enough
       * - If buf is too small, buf is unchanged, however result is still updated
       * - Found key is written into result
       */
      void get_prev_object( get_object_result& result, const get_object_args& args )const;

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
      bool is_writable()const;

      const state_node_id& id()const;
      const state_node_id& parent_id()const;
      uint64_t             revision()const;

      friend class detail::state_db_impl;

   private:
      std::unique_ptr< detail::state_node_impl > impl;
};

using state_node_ptr = std::shared_ptr< state_node >;

/**
 * StateDB is designed to provide parallel access to the database across
 * different states.
 *
 * It does by tracking positive state deltas, which can be merged on the fly
 * at read time to return the correct state of the database. A database
 * checkpoint is represented by the state_node class. Reads and writes happen
 * against a state_node.
 *
 * States are organized as a tree with the assumption that one path wins out
 * over time and cousin paths are discarded as the root is advanced.
 *
 * Currently, state_db is not thread safe. That is, calls directly on state_db
 * are not thread safe. (i.e. deleting a node concurrently to creating a new
 * node can leave statedb in an undefined state)
 *
 * Conccurrency across state nodes is supported native to the implementation
 * without locks. Writes on a single state node need to be serialized, but
 * reads are implicitly parallel.
 *
 * TODO: Either extend the design of statedb to support concurrent access
 * or implement a some locking mechanism for access to the fork multi
 * index container.
 *
 * There is an additional corner case that is difficult to address.
 *
 * Upon squashing a state node, readers may be reading from the node that
 * is being squashed or an intermediate node between root and that node.
 * Relatively speaking, this should happen infrequently (on the order of once
 * per some number of seconds). As such, whatever guarantees concurrency
 * should heavily favor readers. Writing can happen lazily, preferably when
 * there is no contention from readers at all.
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
      state_node_ptr get_empty_node();

      /**
       * Get a list of recent state nodes.
       */
      void get_recent_states(std::vector< state_node_ptr >& node_list, uint64_t limit);

      /**
       * Get an ancestor of a node at a particular revision
       */
      state_node_ptr get_node_at_revision( uint64_t revision, state_node_id& child_id )const;
      state_node_ptr get_node_at_revision( uint64_t revision )const;

      /**
       * Get the state_node for the given state_node_id.
       *
       * Return an empty pointer if no node for the given id exists.
       */
      state_node_ptr get_node( const state_node_id& node_id )const;

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
      state_node_ptr create_writable_node( const state_node_id& parent_id, const state_node_id& new_id );

      /**
       * Finalize a node.  The node will no longer be writable.
       */
      void finalize_node( const state_node_id& node_id );

      /**
       * Discard the node, it can no longer be used.
       *
       * If the node has any children, they too will be deleted because
       * there will no longer exist a path from root to those nodes.
       *
       * This will fail if the node you are deleting would cause the
       * current head node to be delted.
       */
      void discard_node( const state_node_id& node_id );

      /**
       * Squash the node in to the root state, committing it.
       * Branching state between this node and its ancestor will be discarded
       * and no longer accesible.
       *
       * It is the responsiblity of the caller to ensure no readers or writers
       * are accessing affected nodes by this call.
       *
       * TODO: Implement thread safety within commit node to make
       * statedb thread safe for all callers.
       */
      void commit_node( const state_node_id& node_id );

      /**
       * Get and return the current "head" node.
       *
       * Head is determined by longest chain. Oldest
       * chain wins in a tie of length. Only finalized
       * nodes are eligible to become head.
       */
      state_node_ptr get_head()const;

      /**
       * Get and return the current "root" node.
       *
       * All state nodes are guaranteed to a descendant of root.
       */
      state_node_ptr get_root()const;

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
