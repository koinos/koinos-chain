
#pragma once
#include <koinos/state_db/state_db_types.hpp>
#include <koinos/state_db/detail/state_delta.hpp>

#include <any>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

namespace koinos::state_db {

namespace detail {

class database_impl;
class state_node_impl;

} // detail

class state_node;

using state_node_ptr = std::shared_ptr< state_node >;

/**
 * Allows querying the database at a particular checkpoint.
 */
class state_node
{
   public:
      state_node() = default;
      state_node( detail::state_delta_ptr );
      ~state_node() = default;

      /**
       * Fetch an object if one exists.
       *
       * - Size of the object is written into result.size
       * - Object's value is copied into args.buf, provided buf != nullptr and buf_size is large enough
       * - If buf is too small, buf is unchanged, however result is still updated
       * - args.key is copied into result.key
       */
      const object_value* get_object( const object_space& space, const object_key& key ) const;

      /**
       * Get the next object.
       *
       * - Size of the object is written into result.size
       * - Object's value is copied into args.buf, provided buf != nullptr and buf_size is large enough
       * - If buf is too small, buf is unchanged, however result is still updated
       * - Found key is written into result
       */
      std::pair< const object_value*, const object_key > get_next_object( const object_space& space, const object_key& key ) const;

      /**
       * Get the previous object.
       *
       * - Size of the object is written into result.size
       * - Object's value is copied into args.buf, provided buf != nullptr and buf_size is large enough
       * - If buf is too small, buf is unchanged, however result is still updated
       * - Found key is written into result
       */
      std::pair< const object_value*, const object_key > get_prev_object( const object_space& space, const object_key& key ) const;

      /**
       * Write an object into the state_node.
       *
       * - Fail if node is not writable.
       * - If object exists, object is overwritten.
       */
      int32_t put_object( const object_space& space, const object_key& key, const object_value* val );

      /**
       * Remove an object from the state_node
       */
      void remove_object( const object_space& space, const object_key& key );

      /**
       * Return true if the node is writable.
       */
      bool is_writable() const;

      /**
       * Return the merkle root of writes on this state node
       */
      crypto::multihash get_merkle_root() const;

      /**
       * Returns an anonymous state node with this node as its parent.
       */
      state_node_ptr create_anonymous_node();

      const state_node_id& id() const;
      const state_node_id& parent_id() const;
      uint64_t             revision() const;
      state_node_ptr       get_parent() const;

      void commit();
      //void reset();

      friend class detail::database_impl;

   private:
      detail::state_delta_ptr _state;
};

/**
 * database is designed to provide parallel access to the database across
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
 * Currently, database is not thread safe. That is, calls directly on database
 * are not thread safe. (i.e. deleting a node concurrently to creating a new
 * node can leave database in an undefined state)
 *
 * Conccurrency across state nodes is supported native to the implementation
 * without locks. Writes on a single state node need to be serialized, but
 * reads are implicitly parallel.
 *
 * TODO: Either extend the design of database to support concurrent access
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
class database final
{
   public:
      database();
      ~database();

      /**
       * Open the database.
       */
      void open( const std::filesystem::path& p, std::function< void( state_node_ptr ) > init = nullptr );

      /**
       * Close the database.
       */
      void close();

      /**
       * Reset the database.
       */
      void reset();

      /**
       * Get an ancestor of a node at a particular revision
       */
      state_node_ptr get_node_at_revision( uint64_t revision, const state_node_id& child_id ) const;
      state_node_ptr get_node_at_revision( uint64_t revision ) const;

      /**
       * Get the state_node for the given state_node_id.
       *
       * Return an empty pointer if no node for the given id exists.
       */
      state_node_ptr get_node( const state_node_id& node_id ) const;

      /**
       * Create a writable state_node.
       *
       * - If parent_id refers to a writable node, fail.
       * - Otherwise, return a new writable node.
       * - Writing to the returned node will not modify the parent node.
       *
       * If the parent is subsequently discarded, database preserves
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
       * database thread safe for all callers.
       */
      void commit_node( const state_node_id& node_id );

      /**
       * Get and return the current "head" node.
       *
       * Head is determined by longest chain. Oldest
       * chain wins in a tie of length. Only finalized
       * nodes are eligible to become head.
       */
      state_node_ptr get_head() const;

      /**
       * Get and return a vector of all fork heads.
       *
       * Fork heads are any finalized nodes that do
       * not have children.
       */
      std::vector< state_node_ptr > get_fork_heads() const;

      /**
       * Get and return the current "root" node.
       *
       * All state nodes are guaranteed to a descendant of root.
       */
      state_node_ptr get_root() const;

   private:
      std::unique_ptr< detail::database_impl > impl;
};

} // koinos::state_db
