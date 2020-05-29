
#include <koinos/pack/rt/string.hpp>

#include <koinos/statedb/koinos_object_types.hpp>
#include <koinos/statedb/merge_iterator.hpp>
#include <koinos/statedb/state_delta.hpp>
#include <koinos/statedb/statedb.hpp>
#include <koinos/statedb/objects.hpp>

#include <cstring>
#include <deque>
#include <optional>
#include <utility>

namespace koinos { namespace statedb {

namespace detail {

struct by_id;
struct by_revision;
struct by_parent;

using state_multi_index_type = boost::multi_index_container<
   state_node_ptr,
   boost::multi_index::indexed_by<
      boost::multi_index::ordered_unique<
         boost::multi_index::tag< by_id >,
            boost::multi_index::const_mem_fun< state_node, const state_node_id&, &state_node::id >
      >,
      boost::multi_index::ordered_non_unique<
         boost::multi_index::tag< by_parent >,
            boost::multi_index::const_mem_fun< state_node, const state_node_id&, &state_node::parent_id >
      >,
      boost::multi_index::ordered_non_unique<
         boost::multi_index::tag< by_revision >,
            boost::multi_index::const_mem_fun< state_node, uint64_t, &state_node::revision >
      >
   >
>;

using state_delta_type = state_delta< state_object_index >;
using state_delta_ptr = std::shared_ptr< state_delta_type >;

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

      void init( state_delta_ptr _state );

      void get_object( get_object_result& result, const get_object_args& args )const;
      void get_next_object( get_object_result& result, const get_object_args& args )const;
      void get_prev_object( get_object_result& result, const get_object_args& args )const;
      void put_object( put_object_result& result, const put_object_args& args );

      state_delta_ptr   _state;
      bool              _is_writable;

      // This dequeue contains pointers to all state deltas between the current
      // state and this state for use in merge queries. However, when state is
      // squashed, these pointers become invalidated. Storing as weak pointers
      // lets them be locked when a read is occurring, guaranteeing the
      // resource is not released prematurely and ensuring the correct merge
      // query is always calculated.
      boost::container::deque< std::weak_ptr< state_delta_type > >   _delta_deque;
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
      ~state_db_impl() { close(); }

      void open( const boost::filesystem::path& p, const boost::any& o );
      void close();

      state_node_ptr get_empty_node();
      void get_recent_states( std::vector< state_node_ptr >& get_recent_nodes, uint64_t limit );
      state_node_ptr get_node_at_revision( uint64_t revision, const state_node_id& child )const;
      state_node_ptr get_node( const state_node_id& node_id )const;
      state_node_ptr create_writable_node( const state_node_id& parent_id, const state_node_id& new_id );
      void finalize_node( const state_node_id& node );
      void discard_node( const state_node_id& node, const flat_set< state_node_id >& whitelist );
      void commit_node( const state_node_id& node );

      state_node_ptr get_head()const;
      state_node_ptr get_root()const;

      bool is_open()const;

      boost::filesystem::path      _path;
      boost::any                   _options;

      state_multi_index_type       _index;
      state_node_ptr               _head;
      state_node_ptr               _root;
};

state_node_ptr state_db_impl::get_empty_node()
{
   //
   // This method closes, wipes and re-opens the database.
   //
   // So the caller needs to be very careful to only call this method if deleting the database is desirable!
   //

   KOINOS_ASSERT( is_open(), database_not_open, "Database is not open", () );
   // Wipe and start over from empty database!
   _root->impl->_state->clear();
   close();
   open( _path, _options );

   return _head;
}

void state_db_impl::open( const boost::filesystem::path& p, const boost::any& o )
{
   auto root = std::make_shared< state_node >();
   root->impl->init( std::make_shared< state_delta_type >( p, o ) );
   root->impl->_is_writable = false;
   _index.insert( root );
   _root = root;
   _head = root;

   _path = p;
   _options = o;
}

void state_db_impl::close()
{
   _root.reset();
   _head.reset();
   _index.clear();
}

void state_db_impl::get_recent_states( std::vector< state_node_ptr >& node_list, uint64_t limit )
{
   KOINOS_ASSERT( is_open(), database_not_open, "Database is not open", () );
   node_list.clear();
   node_list.reserve( limit );
   auto node_itr = _index.find( _head->id() );

   while( node_list.size() < limit && node_itr != _index.end() )
   {
      node_list.emplace_back( *node_itr );
      if( *node_itr == _root ) return;

      node_itr = _index.find( (*node_itr)->parent_id() );
   }
}

state_node_ptr state_db_impl::get_node_at_revision( uint64_t revision, const state_node_id& child_id )const
{
   KOINOS_ASSERT( is_open(), database_not_open, "Database is not open", () );
   KOINOS_ASSERT( revision >= _root->revision(), illegal_argument,
      "Cannot ask for node with revision less than root. root rev: ${root}, requested: ${req}",
      ("root", _root->revision())("req", revision) );

   if( revision == _root->revision() ) return _root;

   auto child = get_node( child_id );
   if( !child ) child = _head;

   state_delta_ptr delta = child->impl->_state;

   while( delta->revision() > revision )
   {
      delta = delta->parent();
   }

   auto node_itr = _index.find( delta->id() );
   KOINOS_ASSERT( node_itr != _index.end(), internal_error,
      "Could not find state node associated with linked state_delta ${id}", ("id", delta->id()) );
   return *node_itr;
}

state_node_ptr state_db_impl::get_node( const state_node_id& node_id )const
{
   KOINOS_ASSERT( is_open(), database_not_open, "Database is not open", () );

   auto node_itr = _index.find( node_id );
   return node_itr != _index.end() ? *node_itr : state_node_ptr();
}

state_node_ptr state_db_impl::create_writable_node( const state_node_id& parent_id, const state_node_id& new_id )
{
   KOINOS_ASSERT( is_open(), database_not_open, "Database is not open", () );

   auto parent_state = _index.find( parent_id );
   if( parent_state != _index.end() && !(*parent_state)->is_writable() )
   {
      auto node = std::make_shared< state_node >();
      node->impl->init( std::make_shared< state_delta_type >( (*parent_state)->impl->_state, new_id ) );
      if( _index.insert( node ).second )
         return node;
   }

   return state_node_ptr();
}

void state_db_impl::finalize_node( const state_node_id& node_id )
{
   KOINOS_ASSERT( is_open(), database_not_open, "Database is not open", () );
   auto node = get_node( node_id );
   KOINOS_ASSERT( node, illegal_argument, "Node ${n} not found.", ("n", node_id) );

   node->impl->_is_writable = false;

   if( node->revision() > _head->revision() )
   {
      _head = node;
   }
}

void state_db_impl::discard_node( const state_node_id& node_id, const flat_set< state_node_id >& whitelist )
{
   KOINOS_ASSERT( is_open(), database_not_open, "Database is not open", () );
   auto node = get_node( node_id );

   if( !node ) return;

   KOINOS_ASSERT( node_id != _root->id(), illegal_argument, "Cannot discard root node" );

   std::vector< state_node_id > remove_queue{ node_id };
   const auto& previdx = _index.template get< by_parent >();
   const auto head_id = _head->id();

   for( uint32_t i = 0; i < remove_queue.size(); ++i )
   {
      KOINOS_ASSERT( remove_queue[ i ] != head_id, cannot_discard, "Cannot discard a node that would result in discarding of head", () );

      auto previtr = previdx.lower_bound( remove_queue[ i ] );
      while ( previtr != previdx.end() && (*previtr)->parent_id() == remove_queue[ i ] )
      {
         // Do not remove nodes on the whitelist
         if( whitelist.find( (*previtr)->id() ) == whitelist.end() )
         {
            remove_queue.push_back( (*previtr)->id() );
         }

         ++previtr;
      }
   }

   for( const auto& id : remove_queue )
   {
      auto itr = _index.find( id );
      if ( itr != _index.end() )
         _index.erase( itr );
   }
}

void state_db_impl::commit_node( const state_node_id& node_id )
{
   KOINOS_ASSERT( is_open(), database_not_open, "Database is not open", () );
   KOINOS_ASSERT( node_id != _root->id(), illegal_argument, "Cannot commit root node. Root node already committed." );
   auto node = get_node( node_id );
   KOINOS_ASSERT( node, illegal_argument, "Node ${n} not found.", ("n", node_id) );

   flat_set< state_node_id > whitelist{ node->id() };

   auto old_root = _root;
   _root = node;
   _index.modify( _index.find( node->id() ), []( state_node_ptr& n ){ n->impl->_state->commit(); } );
   discard_node( old_root->id(), whitelist );
}

state_node_ptr state_db_impl::get_head()const
{
   KOINOS_ASSERT( is_open(), database_not_open, "Database is not open", () );
   return _head;
}

state_node_ptr state_db_impl::get_root()const
{
   KOINOS_ASSERT( is_open(), database_not_open, "Database is not open", () );
   return _root;
}

bool state_db_impl::is_open()const
{
   return (bool)_root && (bool)_head;
}

void state_node_impl::init( state_delta_ptr state )
{
   _state = state;

   auto state_delta_ptr = _state;

   while( !state_delta_ptr->is_root() )
   {
      _delta_deque.emplace_front( state_delta_ptr );
      state_delta_ptr = state_delta_ptr->parent();
   }

   _delta_deque.emplace_front( state_delta_ptr );
}

void state_node_impl::get_object( get_object_result& result, const get_object_args& args )const
{
   auto idx = merge_index< state_object_index, by_key >( _delta_deque );
   auto pobj = idx.find( boost::make_tuple( args.space, args.key ) );
   if( pobj != idx.end() )
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

void state_node_impl::get_next_object( get_object_result& result, const get_object_args& args )const
{
   auto idx = merge_index< state_object_index, by_key >( _delta_deque );
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

void state_node_impl::get_prev_object( get_object_result& result, const get_object_args& args )const
{
   auto idx = merge_index< state_object_index, by_key >( _delta_deque );
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
   KOINOS_ASSERT( _is_writable, node_finalized, "Cannot write to a finalized node", () );
   auto idx = merge_index< state_object_index, by_key >( _delta_deque );
   auto  pobj = idx.find( boost::make_tuple( args.space, args.key ) );
   if( pobj != idx.end() )
   {
      result.object_existed = true;
      if( args.buf != nullptr )
      {
         // exist -> exist, modify()
         _state->modify( *pobj, [&]( state_object& obj )
         {
            obj.value.resize( args.object_size );
            std::memcpy( obj.value.data(), args.buf, args.object_size );
         });
      }
      else
      {
         // exist -> dne, remove()
         _state->erase( *pobj );
      }
   }
   else
   {
      result.object_existed = false;
      if( args.buf != nullptr )
      {
         // dne - exist, create()
         _state->emplace( [&]( state_object& obj )
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

void state_node::get_object( get_object_result& result, const get_object_args& args )const
{
   impl->get_object( result, args );
}

void state_node::get_next_object( get_object_result& result, const get_object_args& args )const
{
   impl->get_next_object( result, args );
}

void state_node::get_prev_object( get_object_result& result, const get_object_args& args )const
{
   impl->get_prev_object( result, args );
}

void state_node::put_object( put_object_result& result, const put_object_args& args )
{
   impl->put_object( result, args );
}

bool state_node::is_writable()const
{
   return impl->_is_writable;
}

const state_node_id& state_node::id()const
{
   return impl->_state->id();
}

const state_node_id& state_node::parent_id()const
{
   return impl->_state->parent_id();
}

uint64_t state_node::revision()const
{
   return impl->_state->revision();
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

state_node_ptr state_db::get_empty_node()
{
   return impl->get_empty_node();
}

void state_db::get_recent_states( std::vector<state_node_ptr>& node_list, uint64_t limit )
{
   impl->get_recent_states( node_list, limit );
}

state_node_ptr state_db::get_node_at_revision( uint64_t revision, state_node_id& child_id )const
{
   return impl->get_node_at_revision( revision, child_id );
}

state_node_ptr state_db::get_node_at_revision( uint64_t revision )const
{
   static const state_node_id null_id;
   return impl->get_node_at_revision( revision, null_id );
}

state_node_ptr state_db::get_node( const state_node_id& node_id )const
{
   return impl->get_node( node_id );
}

state_node_ptr state_db::create_writable_node( const state_node_id& parent_id, const state_node_id& new_id )
{
   return impl->create_writable_node( parent_id, new_id );
}

void state_db::finalize_node( const state_node_id& node_id )
{
   impl->finalize_node( node_id );
}

void state_db::discard_node( const state_node_id& node_id )
{
   static const flat_set< state_node_id > whitelist;
   impl->discard_node( node_id, whitelist );
}

void state_db::commit_node( const state_node_id& node_id )
{
   impl->commit_node( node_id );
}

state_node_ptr state_db::get_head()const
{
   return impl->get_head();
}

state_node_ptr state_db::get_root()const
{
   return impl->get_root();
}

} } // koinos::state_db
