
#include <koinos/statedb/koinos_object_types.hpp>
#include <koinos/statedb/statedb.hpp>
#include <koinos/statedb/objects.hpp>

#include <cstring>
#include <deque>
#include <optional>
#include <utility>

namespace koinos { namespace statedb {

namespace detail {

/**
 * Private implementation of state_node interface.
 *
 * Maintains a pointer to state_db_impl,
 * only allows reads / writes if the node corresponds to the DB's current state.
 */
class state_node_impl final
{
   public:
      state_node_impl();
      ~state_node_impl();

      void get_object( get_object_result& result, const get_object_args& args );
      void get_next_object( get_object_result& result, const get_object_args& args );
      void get_prev_object( get_object_result& result, const get_object_args& args );
      void put_object( put_object_result& result, const put_object_args& args );
      bool is_tip();

      state_db_impl*           _state_db = nullptr;
      int64_t                  _node_id  = 0;
      bool                     _is_writable = false;

      /**
       * The chainbase session corresponding to this node.
       *
       * This is always initialized by make_node().  It cannot be initialized
       * in the ctor due to data flow / encapsulation breaking issues.
       *
       * The only reason it is optional is to make it "safe" to leave
       * uninitialized until it is set in make_node().  Since nodes
       * are always initialized by make_node(), it is safe for code outside
       * of make_node() to assume _session contains a value.
       */
      std::optional< chainbase::database::session > _session;
};

state_node_impl::state_node_impl() {}
state_node_impl::~state_node_impl() {}

/**
 * Private implementation of state_db interface.
 *
 * This class relies heavily on using chainbase as a backing store.
 * Only one state_node can be active at a time.  This is enforced by
 * _current_node which is a weak_ptr.  New nodes will throw
 * NodeNotExpired exception if a
 */
class state_db_impl final
{
   public:
      state_db_impl() {}
      ~state_db_impl() {}

      void open( const boost::filesystem::path& p, const boost::any& o );
      void close();

      std::shared_ptr< state_node > get_empty_node();
      void get_recent_states(std::vector<state_node_id>& get_recent_nodes, int limit);
      std::shared_ptr< state_node > get_node( state_node_id node_id );
      std::shared_ptr< state_node > create_writable_node( state_node_id parent_id );
      void finalize_node( state_node_id node_id );
      void discard_node( state_node_id node_id );

      /**
       * Create a new state_node and add it to the tip.
       */
      std::shared_ptr< state_node > make_node( bool is_writable );

      /**
       * Get the tip node.  If the node ID refers to a node other than
       * the tip node, throw bad_node_position.
       */
      std::shared_ptr< state_node > get_tip( state_node_id node_id );

      /**
       * Require the tip node to have the given node_id.
       * If the node ID refers to a node other than the tip node, throw bad_node_position.
       */
      void require_tip( state_node_id node_id );

      chainbase::database          _chainbase_db;
      boost::filesystem::path      _chainbase_path;
      boost::any                   _chainbase_options;
      bool                         _is_open = false;
      std::deque< std::shared_ptr< state_node > > _state_nodes;
      state_node_id                _next_node_id = 1;
};

std::shared_ptr< state_node > state_db_impl::make_node(bool is_writable)
{
   std::shared_ptr< state_node > node = std::make_shared< state_node >();
   node->impl->_state_db = this;
   node->impl->_node_id = _next_node_id;
   node->impl->_is_writable = is_writable;
   node->impl->_session.emplace( std::move( _chainbase_db.start_undo_session() ) );
   ++_next_node_id;
   _state_nodes.push_back( node );
   return node;
}

std::shared_ptr< state_node > state_db_impl::get_tip( state_node_id node_id )
{
   KOINOS_ASSERT( _is_open, database_not_open, "Database is not open", () );
   KOINOS_ASSERT( node_id >= 0, illegal_argument, "node_id is negative", () );

   size_t n = _state_nodes.size();
   KOINOS_ASSERT( n > 0, internal_error, "_state_nodes is empty", () );
   std::shared_ptr< state_node >& tip_node = _state_nodes.back();
   KOINOS_ASSERT( tip_node, internal_error, "tip_node contains a null pointer", () );
   if( tip_node->node_id() == node_id )
      return tip_node;
   // We will throw an exception at this point, all that's left is to figure out
   //    if it's bad_node_position or unknown_node.
   std::shared_ptr< state_node > node = get_node( node_id );
   KOINOS_ASSERT( node, unknown_node, "Node does not exist", () );
   KOINOS_THROW( bad_node_position, "Node is not tip node", () );
}

void state_db_impl::require_tip( state_node_id node_id )
{
   KOINOS_ASSERT( _is_open, database_not_open, "Database is not open", () );
   KOINOS_ASSERT( node_id >= 0, illegal_argument, "node_id is negative", () );

   size_t n = _state_nodes.size();
   KOINOS_ASSERT( n > 0, internal_error, "_state_nodes is empty", () );
   std::shared_ptr< state_node >& tip_node = _state_nodes.back();
   KOINOS_ASSERT( tip_node, internal_error, "tip_node contains a null pointer", () );
   if( tip_node->node_id() == node_id )
      return;
   // We will throw an exception at this point, all that's left is to figure out
   //    if it's bad_node_position or unknown_node.
   std::shared_ptr< state_node > node = get_node( node_id );
   KOINOS_ASSERT( node, unknown_node, "Node does not exist", () );
   KOINOS_THROW( bad_node_position, "Node is not tip node", () );
}

std::shared_ptr< state_node > state_db_impl::get_empty_node()
{
   //
   // This method closes, wipes and re-opens the chainbase database.
   //
   // So the caller needs to be very careful to only call this method if deleting the database is desirable!
   //

   KOINOS_ASSERT( _is_open, database_not_open, "Database is not open", () );
   // Wipe chainbase and start over from empty database!
   _chainbase_db.close();
   _is_open = false;
   _chainbase_db.wipe( _chainbase_path );
   _chainbase_db.open( _chainbase_path, 0, _chainbase_options );

   return make_node(false);
}

void state_db_impl::open( const boost::filesystem::path& p, const boost::any& o )
{
   _chainbase_db.open(p, 0, o);
   _chainbase_path = p;
   _chainbase_options = o;
   _chainbase_db.add_index< state_object_index >();

   _is_open = true;
   // Make a node to represent the initial state of the database
   // This node's session will be empty (i.e. popping the session does nothing)
   make_node(false);
}

void state_db_impl::close()
{
   _chainbase_db.close();
   _is_open = false;
}

void state_db_impl::get_recent_states(std::vector<state_node_id>& node_id_list, int limit)
{
   KOINOS_ASSERT( _is_open, database_not_open, "Database is not open", () );
   int n = _state_nodes.size();
   limit = std::min( limit, n );
   for( int i=0; i<limit; i++ )
      node_id_list.push_back( _state_nodes[(n-1)-i]->node_id() );
}

std::shared_ptr< state_node > state_db_impl::get_node( state_node_id node_id )
{
   KOINOS_ASSERT( _is_open, database_not_open, "Database is not open", () );
   KOINOS_ASSERT( node_id >= 0, illegal_argument, "node_id is negative", () );

   size_t n = _state_nodes.size();
   KOINOS_ASSERT( n > 0, internal_error, "_state_nodes is empty", () );
   for( size_t i=0; i<n; i++ )
   {
      std::shared_ptr< state_node >& current_node = _state_nodes[(n-1)-i];
      KOINOS_ASSERT( current_node, internal_error, "_state_nodes contains a null pointer", () );
      if( current_node->node_id() == node_id )
         return current_node;
   }
   return std::shared_ptr< state_node >();
}

std::shared_ptr< state_node > state_db_impl::create_writable_node( state_node_id parent_id )
{
   KOINOS_ASSERT( _is_open, database_not_open, "Database is not open", () );
   KOINOS_ASSERT( parent_id >= 0, illegal_argument, "parent_id is negative", () );

   std::shared_ptr< state_node > parent_node = get_tip( parent_id );
   KOINOS_ASSERT( !parent_node->is_writable(), bad_node_position, "Parent is writable", () );

   return make_node(true);
}

void state_db_impl::finalize_node( state_node_id node_id )
{
   KOINOS_ASSERT( _is_open, database_not_open, "Database is not open", () );
   KOINOS_ASSERT( node_id >= 0, illegal_argument, "node_id is negative", () );

   std::shared_ptr< state_node > node = get_tip(node_id);
   node->impl->_is_writable = false;
}

void state_db_impl::discard_node( state_node_id node_id )
{
   size_t n = _state_nodes.size();

   KOINOS_ASSERT( _is_open, database_not_open, "Database is not open", () );
   KOINOS_ASSERT( node_id >= 0, illegal_argument, "node_id is negative", () );
   KOINOS_ASSERT( n > 0, cannot_discard, "Cannot discard the last session", () );

   if( n == 1 )
   {
      // Don't actually discard the last session.
      return;
   }

   std::shared_ptr< state_node > back = _state_nodes.back();
   KOINOS_ASSERT( back, internal_error, "_state_nodes contains a null pointer", () );
   KOINOS_ASSERT( back->impl->_session, internal_error, "impl->_session is null", () );
   if( node_id == back->node_id() )
   {
      // Last node, undo()
      back->impl->_session->undo();
      _state_nodes.pop_back();
      return;
   }

   std::shared_ptr< state_node > front = _state_nodes.front();
   KOINOS_ASSERT( front, internal_error, "_state_nodes contains a null pointer", () );
   KOINOS_ASSERT( front->impl->_session, internal_error, "impl->_session is null", () );
   if( node_id == front->node_id() )
   {
      // First node, commit()
      _chainbase_db.commit(front->impl->_session->revision());
      _state_nodes.pop_front();
      return;
   }

   std::shared_ptr< state_node > penultimate = _state_nodes[n-2];
   KOINOS_ASSERT( penultimate, internal_error, "_state_nodes contains a null pointer", () );
   KOINOS_ASSERT( penultimate->impl->_session, internal_error, "impl->_session is null", () );
   if( node_id == penultimate->node_id() )
   {
      // Penultimate node, squash()
      //
      // This is a complicated case.  The semantics presented to the caller are that
      // `back` continues to exist while `penultimate` is discarded.  However, the
      // underlying chainbase implementation instead has the semantics of discarding
      // the final session and merging into the prior session.
      //

      back->impl->_session->squash();

      // Before:  ... pen back
      _state_nodes.pop_back();
      _state_nodes[n-2] = back;
      // After:   ... back

      back->impl->_session.emplace( std::move( *penultimate->impl->_session ) );
      return;
   }

   KOINOS_THROW( bad_node_position, "Can only discard front, back or penultimate", () );
}

void state_node_impl::get_object( get_object_result& result, const get_object_args& args )
{
   KOINOS_ASSERT( _state_db, internal_error, "_state_db is null", () );
   _state_db->require_tip( _node_id );
   const state_object* pobj = _state_db->_chainbase_db.find< state_object, ByKey >( boost::make_tuple( args.space, args.key ) );
   if( pobj != nullptr )
   {
      result.key = pobj->key;
      result.size = pobj->value.size();
      if( (args.buf != nullptr) && (args.buf_size > 0) )
      {
         uint64_t size = std::min( uint64_t( result.size ), args.buf_size );
         std::memcpy( args.buf, pobj->value.c_str(), size );
      }
   }
   else
   {
      result.key = object_key();
      result.size = -1;
   }
}

void state_node_impl::get_next_object( get_object_result& result, const get_object_args& args )
{
   KOINOS_ASSERT( _state_db, internal_error, "_state_db is null", () );
   _state_db->require_tip( _node_id );
   chainbase::database &db = _state_db->_chainbase_db;

   const auto& idx = db.get_index< state_object_index, ByKey >();
   auto it = idx.upper_bound( boost::make_tuple( args.space, args.key ) );
   if( (it != idx.end()) && (it->space == args.space) )
   {
      result.key = it->key;
      result.size = it->value.size();
      if( (args.buf != nullptr) && (args.buf_size > 0) )
      {
         uint64_t size = std::min( uint64_t( result.size ), args.buf_size );
         std::memcpy( args.buf, it->value.c_str(), size );
      }
   }
   else
   {
      result.key = object_key();
      result.size = -1;
   }
}

void state_node_impl::get_prev_object( get_object_result& result, const get_object_args& args )
{
   KOINOS_ASSERT( _state_db, internal_error, "_state_db is null", () );
   _state_db->require_tip( _node_id );
   chainbase::database &db = _state_db->_chainbase_db;

   const auto& idx = db.get_index< state_object_index, ByKey >();
   auto it = idx.lower_bound( boost::make_tuple( args.space, args.key ) );
   if( it != idx.begin() )
   {
      --it;
      if( it->space == args.space )
      {
         result.key = it->key;
         result.size = it->value.size();
         if( (args.buf != nullptr) && (args.buf_size > 0) )
         {
            uint64_t size = std::min( uint64_t( result.size ), args.buf_size );
            std::memcpy( args.buf, it->value.c_str(), size );
         }
         return;
      }
   }
   result.key = object_key();
   result.size = -1;
}

void state_node_impl::put_object( put_object_result& result, const put_object_args& args )
{
   KOINOS_ASSERT( _state_db, internal_error, "_state_db is null", () );
   _state_db->require_tip( _node_id );
   chainbase::database &db = _state_db->_chainbase_db;

   const state_object* pobj = _state_db->_chainbase_db.find< state_object, ByKey >( boost::make_tuple( args.space, args.key ) );
   if( pobj != nullptr )
   {
      result.object_existed = true;
      if( args.buf != nullptr )
      {
         // exist -> exist, modify()
         db.modify( *pobj, [&]( state_object& obj )
         {
            obj.value.resize( args.object_size );
            std::memcpy( obj.value.data(), args.buf, args.object_size );
         } );
      }
      else
      {
         // exist -> dne, remove()
         db.remove( *pobj );
      }
   }
   if( pobj == nullptr )
   {
      result.object_existed = false;
      if( args.buf != nullptr )
      {
         // dne - exist, create()
         db.create< state_object >( [&]( state_object& obj )
         {
            obj.space = args.space;
            obj.key = args.key;
            obj.value.resize( args.object_size );
            std::memcpy( obj.value.data(), args.buf, args.object_size );
         });
      }
      else
      {
         // dne -> dne, do nothing
      }
   }
}

} // detail

state_node::state_node() : impl( new detail::state_node_impl() ) {}
state_node::~state_node() {}

void state_node::get_object( get_object_result& result, const get_object_args& args )
{
   impl->get_object( result, args );
}

void state_node::get_next_object( get_object_result& result, const get_object_args& args )
{
   impl->get_next_object( result, args );
}

void state_node::get_prev_object( get_object_result& result, const get_object_args& args )
{
   impl->get_prev_object( result, args );
}

void state_node::put_object( put_object_result& result, const put_object_args& args )
{
   impl->put_object( result, args );
}

bool state_node::is_writable()
{
   return impl->_is_writable;
}

state_node_id state_node::node_id()
{
   return impl->_node_id;
}

state_db::state_db() : impl( new detail::state_db_impl() ) {}
state_db::~state_db() {}

void state_db::open( const boost::filesystem::path& p, const boost::any& o )
{
   impl->open( p, o );
}

void state_db::close()
{
   impl->close();
}

std::shared_ptr< state_node > state_db::get_empty_node()
{
   return impl->get_empty_node();
}

void state_db::get_recent_states(std::vector<state_node_id>& node_id_list, int limit)
{
   impl->get_recent_states( node_id_list, limit );
}

std::shared_ptr< state_node > state_db::get_node( state_node_id node_id )
{
   return impl->get_node( node_id );
}

std::shared_ptr< state_node > state_db::create_writable_node( state_node_id parent_id )
{
   return impl->create_writable_node( parent_id );
}

void state_db::finalize_node( state_node_id node_id )
{
   impl->finalize_node( node_id );
}

void state_db::discard_node( state_node_id node_id )
{
   impl->discard_node( node_id );
}

} } // koinos::state_db
