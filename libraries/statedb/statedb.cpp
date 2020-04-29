
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
 * Private implementation of StateNode interface.
 *
 * Maintains a pointer to StateDB_Impl,
 * only allows reads / writes if the node corresponds to the DB's current state.
 */
class StateNode_Impl final
{
   public:
      StateNode_Impl();
      ~StateNode_Impl();

      void get_object( GetObjectResult& result, const GetObjectArgs& args );
      void get_next_object( GetObjectResult& result, const GetObjectArgs& args );
      void get_prev_object( GetObjectResult& result, const GetObjectArgs& args );
      void put_object( PutObjectResult& result, const PutObjectArgs& args );
      bool is_tip();

      StateDB_Impl*            _state_db = nullptr;
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

StateNode_Impl::StateNode_Impl() {}
StateNode_Impl::~StateNode_Impl() {}

/**
 * Private implementation of StateDB interface.
 *
 * This class relies heavily on using chainbase as a backing store.
 * Only one StateNode can be active at a time.  This is enforced by
 * _current_node which is a weak_ptr.  New nodes will throw
 * NodeNotExpired exception if a
 */
class StateDB_Impl final
{
   public:
      StateDB_Impl() {}
      ~StateDB_Impl() {}

      void open( const boost::filesystem::path& p, const boost::any& o );
      void close();

      std::shared_ptr< StateNode > get_empty_node();
      void get_recent_states(std::vector<StateNodeId>& get_recent_nodes, int limit);
      std::shared_ptr< StateNode > get_node( StateNodeId node_id );
      std::shared_ptr< StateNode > create_writable_node( StateNodeId parent_id );
      void finalize_node( StateNodeId node_id );
      void discard_node( StateNodeId node_id );

      /**
       * Create a new StateNode and add it to the tip.
       */
      std::shared_ptr< StateNode > make_node( bool is_writable );

      /**
       * Get the tip node.  If the node ID refers to a node other than
       * the tip node, throw BadNodePosition.
       */
      std::shared_ptr< StateNode > get_tip( StateNodeId node_id );

      /**
       * Require the tip node to have the given node_id.
       * If the node ID refers to a node other than the tip node, throw BadNodePosition.
       */
      void require_tip( StateNodeId node_id );

      chainbase::database          _chainbase_db;
      boost::filesystem::path      _chainbase_path;
      boost::any                   _chainbase_options;
      bool                         _is_open = false;
      std::deque< std::shared_ptr< StateNode > > _state_nodes;
      StateNodeId                  _next_node_id = 1;
};

std::shared_ptr< StateNode > StateDB_Impl::make_node(bool is_writable)
{
   std::shared_ptr< StateNode > node = std::make_shared< StateNode >();
   node->impl->_state_db = this;
   node->impl->_node_id = _next_node_id;
   node->impl->_is_writable = is_writable;
   node->impl->_session.emplace( std::move( _chainbase_db.start_undo_session() ) );
   ++_next_node_id;
   _state_nodes.push_back( node );
   return node;
}

std::shared_ptr< StateNode > StateDB_Impl::get_tip( StateNodeId node_id )
{
   KOINOS_ASSERT( _is_open, DatabaseNotOpen, "Database is not open", () );
   KOINOS_ASSERT( node_id >= 0, IllegalArgument, "node_id is negative", () );

   size_t n = _state_nodes.size();
   KOINOS_ASSERT( n > 0, InternalError, "_state_nodes is empty", () );
   std::shared_ptr< StateNode >& tip_node = _state_nodes.back();
   KOINOS_ASSERT( tip_node, InternalError, "tip_node contains a null pointer", () );
   if( tip_node->node_id() == node_id )
      return tip_node;
   // We will throw an exception at this point, all that's left is to figure out
   //    if it's BadNodePosition or UnknownNode.
   std::shared_ptr< StateNode > node = get_node( node_id );
   KOINOS_ASSERT( node, UnknownNode, "Node does not exist", () );
   KOINOS_THROW( BadNodePosition, "Node is not tip node", () );
}

void StateDB_Impl::require_tip( StateNodeId node_id )
{
   KOINOS_ASSERT( _is_open, DatabaseNotOpen, "Database is not open", () );
   KOINOS_ASSERT( node_id >= 0, IllegalArgument, "node_id is negative", () );

   size_t n = _state_nodes.size();
   KOINOS_ASSERT( n > 0, InternalError, "_state_nodes is empty", () );
   std::shared_ptr< StateNode >& tip_node = _state_nodes.back();
   KOINOS_ASSERT( tip_node, InternalError, "tip_node contains a null pointer", () );
   if( tip_node->node_id() == node_id )
      return;
   // We will throw an exception at this point, all that's left is to figure out
   //    if it's BadNodePosition or UnknownNode.
   std::shared_ptr< StateNode > node = get_node( node_id );
   KOINOS_ASSERT( node, UnknownNode, "Node does not exist", () );
   KOINOS_THROW( BadNodePosition, "Node is not tip node", () );
}

std::shared_ptr< StateNode > StateDB_Impl::get_empty_node()
{
   //
   // This method closes, wipes and re-opens the chainbase database.
   //
   // So the caller needs to be very careful to only call this method if deleting the database is desirable!
   //

   KOINOS_ASSERT( _is_open, DatabaseNotOpen, "Database is not open", () );
   // Wipe chainbase and start over from empty database!
   _chainbase_db.close();
   _is_open = false;
   _chainbase_db.wipe( _chainbase_path );
   _chainbase_db.open( _chainbase_path, 0, _chainbase_options );

   return make_node(false);
}

void StateDB_Impl::open( const boost::filesystem::path& p, const boost::any& o )
{
   _chainbase_db.open(p, 0, o);
   _chainbase_path = p;
   _chainbase_options = o;
   _chainbase_db.add_index< StateObjectIndex >();

   _is_open = true;
   // Make a node to represent the initial state of the database
   // This node's session will be empty (i.e. popping the session does nothing)
   make_node(false);
}

void StateDB_Impl::close()
{
   _chainbase_db.close();
   _is_open = false;
}

void StateDB_Impl::get_recent_states(std::vector<StateNodeId>& node_id_list, int limit)
{
   KOINOS_ASSERT( _is_open, DatabaseNotOpen, "Database is not open", () );
   int n = _state_nodes.size();
   limit = std::min( limit, n );
   for( int i=0; i<limit; i++ )
      node_id_list.push_back( _state_nodes[(n-1)-i]->node_id() );
}

std::shared_ptr< StateNode > StateDB_Impl::get_node( StateNodeId node_id )
{
   KOINOS_ASSERT( _is_open, DatabaseNotOpen, "Database is not open", () );
   KOINOS_ASSERT( node_id >= 0, IllegalArgument, "node_id is negative", () );

   size_t n = _state_nodes.size();
   KOINOS_ASSERT( n > 0, InternalError, "_state_nodes is empty", () );
   for( size_t i=0; i<n; i++ )
   {
      std::shared_ptr< StateNode >& current_node = _state_nodes[(n-1)-i];
      KOINOS_ASSERT( current_node, InternalError, "_state_nodes contains a null pointer", () );
      if( current_node->node_id() == node_id )
         return current_node;
   }
   return std::shared_ptr< StateNode >();
}

std::shared_ptr< StateNode > StateDB_Impl::create_writable_node( StateNodeId parent_id )
{
   KOINOS_ASSERT( _is_open, DatabaseNotOpen, "Database is not open", () );
   KOINOS_ASSERT( parent_id >= 0, IllegalArgument, "parent_id is negative", () );

   std::shared_ptr< StateNode > parent_node = get_tip( parent_id );
   KOINOS_ASSERT( !parent_node->is_writable(), BadNodePosition, "Parent is writable", () );

   return make_node(true);
}

void StateDB_Impl::finalize_node( StateNodeId node_id )
{
   KOINOS_ASSERT( _is_open, DatabaseNotOpen, "Database is not open", () );
   KOINOS_ASSERT( node_id >= 0, IllegalArgument, "node_id is negative", () );

   std::shared_ptr< StateNode > node = get_tip(node_id);
   node->impl->_is_writable = false;
}

void StateDB_Impl::discard_node( StateNodeId node_id )
{
   size_t n = _state_nodes.size();

   KOINOS_ASSERT( _is_open, DatabaseNotOpen, "Database is not open", () );
   KOINOS_ASSERT( node_id >= 0, IllegalArgument, "node_id is negative", () );
   KOINOS_ASSERT( n > 0, CannotDiscard, "Cannot discard the last session", () );

   if( n == 1 )
   {
      // Don't actually discard the last session.
      return;
   }

   std::shared_ptr< StateNode > back = _state_nodes.back();
   KOINOS_ASSERT( back, InternalError, "_state_nodes contains a null pointer", () );
   KOINOS_ASSERT( back->impl->_session, InternalError, "impl->_session is null", () );
   if( node_id == back->node_id() )
   {
      // Last node, undo()
      back->impl->_session->undo();
      _state_nodes.pop_back();
      return;
   }

   std::shared_ptr< StateNode > front = _state_nodes.front();
   KOINOS_ASSERT( front, InternalError, "_state_nodes contains a null pointer", () );
   KOINOS_ASSERT( front->impl->_session, InternalError, "impl->_session is null", () );
   if( node_id == front->node_id() )
   {
      // First node, commit()
      _chainbase_db.commit(front->impl->_session->revision());
      _state_nodes.pop_front();
      return;
   }

   std::shared_ptr< StateNode > penultimate = _state_nodes[n-2];
   KOINOS_ASSERT( penultimate, InternalError, "_state_nodes contains a null pointer", () );
   KOINOS_ASSERT( penultimate->impl->_session, InternalError, "impl->_session is null", () );
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

   KOINOS_THROW( BadNodePosition, "Can only discard front, back or penultimate", () );
}

void StateNode_Impl::get_object( GetObjectResult& result, const GetObjectArgs& args )
{
   KOINOS_ASSERT( _state_db, InternalError, "_state_db is null", () );
   _state_db->require_tip( _node_id );
   const StateObject* pobj = _state_db->_chainbase_db.find< StateObject, ByKey >( boost::make_tuple( args.space, args.key ) );
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
      result.key = ObjectKey();
      result.size = -1;
   }
}

void StateNode_Impl::get_next_object( GetObjectResult& result, const GetObjectArgs& args )
{
   KOINOS_ASSERT( _state_db, InternalError, "_state_db is null", () );
   _state_db->require_tip( _node_id );
   chainbase::database &db = _state_db->_chainbase_db;

   const auto& idx = db.get_index< StateObjectIndex, ByKey >();
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
      result.key = ObjectKey();
      result.size = -1;
   }
}

void StateNode_Impl::get_prev_object( GetObjectResult& result, const GetObjectArgs& args )
{
   KOINOS_ASSERT( _state_db, InternalError, "_state_db is null", () );
   _state_db->require_tip( _node_id );
   chainbase::database &db = _state_db->_chainbase_db;

   const auto& idx = db.get_index< StateObjectIndex, ByKey >();
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
   result.key = ObjectKey();
   result.size = -1;
}

void StateNode_Impl::put_object( PutObjectResult& result, const PutObjectArgs& args )
{
   KOINOS_ASSERT( _state_db, InternalError, "_state_db is null", () );
   _state_db->require_tip( _node_id );
   chainbase::database &db = _state_db->_chainbase_db;

   const StateObject* pobj = _state_db->_chainbase_db.find< StateObject, ByKey >( boost::make_tuple( args.space, args.key ) );
   if( pobj != nullptr )
   {
      result.object_existed = true;
      if( args.buf != nullptr )
      {
         // exist -> exist, modify()
         db.modify( *pobj, [&]( StateObject& obj )
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
         db.create< StateObject >( [&]( StateObject& obj )
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

StateNode::StateNode() : impl( new detail::StateNode_Impl() ) {}
StateNode::~StateNode() {}

void StateNode::get_object( GetObjectResult& result, const GetObjectArgs& args )
{
   impl->get_object( result, args );
}

void StateNode::get_next_object( GetObjectResult& result, const GetObjectArgs& args )
{
   impl->get_next_object( result, args );
}

void StateNode::get_prev_object( GetObjectResult& result, const GetObjectArgs& args )
{
   impl->get_prev_object( result, args );
}

void StateNode::put_object( PutObjectResult& result, const PutObjectArgs& args )
{
   impl->put_object( result, args );
}

bool StateNode::is_writable()
{
   return impl->_is_writable;
}

StateNodeId StateNode::node_id()
{
   return impl->_node_id;
}

StateDB::StateDB() : impl( new detail::StateDB_Impl() ) {}
StateDB::~StateDB() {}

void StateDB::open( const boost::filesystem::path& p, const boost::any& o )
{
   impl->open( p, o );
}

void StateDB::close()
{
   impl->close();
}

std::shared_ptr< StateNode > StateDB::get_empty_node()
{
   return impl->get_empty_node();
}

void StateDB::get_recent_states(std::vector<StateNodeId>& node_id_list, int limit)
{
   impl->get_recent_states( node_id_list, limit );
}

std::shared_ptr< StateNode > StateDB::get_node( StateNodeId node_id )
{
   return impl->get_node( node_id );
}

std::shared_ptr< StateNode > StateDB::create_writable_node( StateNodeId parent_id )
{
   return impl->create_writable_node( parent_id );
}

void StateDB::finalize_node( StateNodeId node_id )
{
   impl->finalize_node( node_id );
}

void StateDB::discard_node( StateNodeId node_id )
{
   impl->discard_node( node_id );
}

} } // koinos::statedb
