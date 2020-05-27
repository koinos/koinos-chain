
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

using state_delta_type = state_delta< state_object_index >;
using state_ptr = std::shared_ptr< state_delta_type >;

struct by_id;
struct by_revision;
struct by_parent;

using state_multi_index_type = boost::multi_index_container<
   state_ptr,
   boost::multi_index::indexed_by<
      boost::multi_index::ordered_unique<
         boost::multi_index::tag< by_id >,
            boost::multi_index::const_mem_fun< state_delta_type, uint256_t, &state_delta_type::state_id >
      >,
      boost::multi_index::ordered_non_unique<
         boost::multi_index::tag< by_parent >,
            boost::multi_index::const_mem_fun< state_delta_type, uint256_t, &state_delta_type::parent_id >
      >,
      boost::multi_index::ordered_non_unique<
         boost::multi_index::tag< by_revision >,
            boost::multi_index::const_mem_fun< state_delta_type, uint64_t, &state_delta_type::revision >
      >
   >
>;

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

      void init( state_db_impl* state_db, state_ptr _state );

      void get_object( get_object_result& result, const get_object_args& args );
      void get_next_object( get_object_result& result, const get_object_args& args );
      void get_prev_object( get_object_result& result, const get_object_args& args );
      void put_object( put_object_result& result, const put_object_args& args );

      state_db_impl*                         _state_db = nullptr;
      state_ptr                              _state;
      boost::container::deque< state_ptr >   _delta_deque;
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
      std::shared_ptr< state_node > create_writable_node( state_node_id parent_id, state_node_id new_id );
      void finalize_node( state_node_id node_id );
      void discard_node( state_node_id node_id, flat_set< state_node_id >& whitelist );
      void commit_node( state_node_id node_id );

      /**
       * Create a new state_node and add it to the tip.
       */
      std::shared_ptr< state_node > make_node( std::shared_ptr< state_node > parent, state_node_id new_id );

      /**
       * Get the tip node.  If the node ID refers to a node other than
       * the tip node, throw bad_node_position.
       */
      std::shared_ptr< state_node > get_tip( state_node_id node_id );

      bool is_open()const;

      /**
       * Require the tip node to have the given node_id.
       * If the node ID refers to a node other than the tip node, throw bad_node_position.
       */
      void require_tip( state_node_id node_id );

      boost::filesystem::path      _path;
      boost::any                   _options;

      state_multi_index_type       _index;
      state_ptr                    _head;
      state_ptr                    _root;
};

std::shared_ptr< state_node > state_db_impl::make_node( std::shared_ptr< state_node > parent, state_node_id new_id )
{
   auto node = std::make_shared< state_node >();
   node->impl->init( this, std::make_shared< state_delta_type >( parent->impl->_state, new_id ) );

   _index.insert( node->impl->_state );

   return node;
}

std::shared_ptr< state_node > state_db_impl::get_empty_node()
{
   //
   // This method closes, wipes and re-opens the database.
   //
   // So the caller needs to be very careful to only call this method if deleting the database is desirable!
   //

   KOINOS_ASSERT( is_open(), database_not_open, "Database is not open", () );
   // Wipe and start over from empty database!
   _root->clear();
   close();
   open( _path, _options );

   auto node = std::make_shared< state_node >();
   node->impl->init( this, _head );

   return node;
}

void state_db_impl::open( const boost::filesystem::path& p, const boost::any& o )
{
   _root = std::make_shared< state_delta_type >( p, o );
   _root->finalize();
   _head = _root;
   _index.insert( _root );

   _path = p;
   _options = o;
}

void state_db_impl::close()
{
   _root.reset();
   _head.reset();
   _index.clear();
}

void state_db_impl::get_recent_states(std::vector<state_node_id>& node_id_list, int limit)
{
   /*
   KOINOS_ASSERT( is_open(), database_not_open, "Database is not open", () );
   int n = _state_nodes.size();
   limit = std::min( limit, n );
   for( int i=0; i<limit; i++ )
      node_id_list.push_back( _state_nodes[(n-1)-i]->node_id() );
   */
}

std::shared_ptr< state_node > state_db_impl::get_node( state_node_id node_id )
{
   KOINOS_ASSERT( is_open(), database_not_open, "Database is not open", () );
   KOINOS_ASSERT( node_id >= 0, illegal_argument, "node_id is negative", () );

   auto state_itr = _index.find( node_id );
   if( state_itr != _index.end() )
   {
      auto node = std::make_shared< state_node >();
      node->impl->init( this, *state_itr );

      return node;
   }

   return std::shared_ptr< state_node >();
}

std::shared_ptr< state_node > state_db_impl::create_writable_node( state_node_id parent_id, state_node_id new_id )
{
   KOINOS_ASSERT( is_open(), database_not_open, "Database is not open", () );
   KOINOS_ASSERT( parent_id >= 0, illegal_argument, "parent_id is negative", () );

   std::shared_ptr< state_node > parent_node = get_tip( parent_id );
   KOINOS_ASSERT( !parent_node->is_writable(), bad_node_position, "Parent is writable", () );

   return make_node( parent_node, new_id );
}

void state_db_impl::finalize_node( state_node_id node_id )
{
   KOINOS_ASSERT( is_open(), database_not_open, "Database is not open", () );
   KOINOS_ASSERT( node_id >= 0, illegal_argument, "node_id is negative", () );

   auto state_itr = _index.find( node_id );
   if( state_itr != _index.end() )
   {
      _index.modify( state_itr, []( state_ptr& p )
      {
         p->finalize();
      });
   }
}

void state_db_impl::discard_node( state_node_id node_id, flat_set< state_node_id >& whitelist )
{
   KOINOS_ASSERT( is_open(), database_not_open, "Database is not open", () );
   KOINOS_ASSERT( node_id >= 0, illegal_argument, "node_id is negative", () );

   auto state_itr = _index.find( node_id );
   if( state_itr == _index.end() ) return;

   std::vector< state_node_id > remove_queue{ node_id };
   const auto& previdx = _index.template get< by_parent >();
   const auto head_id = _head->state_id();

   for( uint32_t i = 0; i < remove_queue.size(); ++i )
   {
      KOINOS_ASSERT( remove_queue[ i ] != head_id, internal_error, "removing the block and its descendants would remove the current head block" );

      auto previtr = previdx.lower_bound( remove_queue[ i ] );
      while ( previtr != previdx.end() && (*previtr)->parent_id() == remove_queue[ i ] )
      {
         // Do not remove nodes on the whitelist
         if( whitelist.find( (*previtr)->state_id() ) == whitelist.end() )
         {
            remove_queue.push_back( (*previtr)->state_id() );
         }

         ++previtr;
      }
   }

   for( const auto& node_id : remove_queue )
   {
      auto itr = _index.find( node_id );
      if ( itr != _index.end() )
         _index.erase( itr );
   }
}

void state_db_impl::commit_node( state_node_id node_id )
{
   KOINOS_ASSERT( is_open(), database_not_open, "Database is not open", () );
   KOINOS_ASSERT( node_id >= 0, illegal_argument, "node_id is negative", () );

   auto state_itr = _index.find( node_id );
   KOINOS_ASSERT( state_itr != _index.end(), internal_error, "Node does not exist" );

   flat_set< state_node_id > whitelist{ node_id };

   auto old_root = _root;
   discard_node( old_root->state_id(), whitelist );
   (*state_itr)->commit();
   _root = *state_itr;
}

bool state_db_impl::is_open()const
{
   return (bool)_root;
}

void state_node_impl::init( state_db_impl* state_db, state_ptr state )
{
   _state_db = state_db;
   _state = state;

   auto delta_ptr = _state;
   while( delta_ptr != _state_db->_root )
   {
      _delta_deque.push_front( delta_ptr );
      delta_ptr = delta_ptr->parent();
   }

   // delta_ptr == _root
   _delta_deque.push_front( delta_ptr );
}

void state_node_impl::get_object( get_object_result& result, const get_object_args& args )
{

   KOINOS_ASSERT( _state_db, internal_error, "_state_db is null", () );
   auto idx = index_wrapper< state_object_index, by_key >( _delta_deque );
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

void state_node_impl::get_next_object( get_object_result& result, const get_object_args& args )
{
   KOINOS_ASSERT( _state_db, internal_error, "_state_db is null", () );

   auto idx = index_wrapper< state_object_index, by_key >( _delta_deque );
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

   auto idx = index_wrapper< state_object_index, by_key >( _delta_deque );
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

   auto idx = index_wrapper< state_object_index, by_key >( _delta_deque );
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
   return impl->_state->is_writable();
}

state_node_id state_node::node_id()
{
   return impl->_state->state_id();
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

std::shared_ptr< state_node > state_db::create_writable_node( state_node_id parent_id, state_node_id new_id )
{
   return impl->create_writable_node( parent_id, new_id );
}

void state_db::finalize_node( state_node_id node_id )
{
   impl->finalize_node( node_id );
}

void state_db::discard_node( state_node_id node_id )
{
   flat_set< state_node_id > whitelist;
   impl->discard_node( node_id, whitelist );
}

void state_db::commit_node( state_node_id node_id )
{
   impl->commit_node( node_id );
}

} } // koinos::state_db
