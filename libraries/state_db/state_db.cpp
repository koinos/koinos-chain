
#include <koinos/chain/chain.pb.h>
#include <koinos/exception.hpp>
#include <koinos/state_db/state_db.hpp>
#include <koinos/state_db/detail/merge_iterator.hpp>
#include <koinos/state_db/detail/state_delta.hpp>
#include <koinos/util/conversion.hpp>

#include <cstring>
#include <deque>
#include <optional>
#include <unordered_set>
#include <utility>

namespace std {
   template<>
   struct hash< koinos::crypto::multihash >
   {
      std::size_t operator()( const koinos::crypto::multihash& mh ) const
      {
         static const std::hash< std::string > hash_fn;
         return hash_fn( koinos::util::converter::as< std::string >( mh ) );
      }
   };

}

namespace koinos::chain {
   bool operator==( const object_space& lhs, const object_space& rhs )
   {
      return lhs.system() == rhs.system()
         && lhs.zone() == rhs.zone()
         && lhs.id() == rhs.id();
   }
}

namespace koinos::state_db {

namespace detail {

struct by_id;
struct by_revision;
struct by_parent;

using state_delta_ptr = std::shared_ptr< state_delta >;

using state_multi_index_type = boost::multi_index_container<
   state_delta_ptr,
   boost::multi_index::indexed_by<
      boost::multi_index::ordered_unique<
         boost::multi_index::tag< by_id >,
            boost::multi_index::const_mem_fun< state_delta, const state_node_id&, &state_delta::id >
      >,
      boost::multi_index::ordered_non_unique<
         boost::multi_index::tag< by_parent >,
            boost::multi_index::const_mem_fun< state_delta, const state_node_id&, &state_delta::parent_id >
      >,
      boost::multi_index::ordered_non_unique<
         boost::multi_index::tag< by_revision >,
            boost::multi_index::const_mem_fun< state_delta, uint64_t, &state_delta::revision >
      >
   >
>;

const object_key null_key = object_key();

/**
 * Private implementation of database interface.
 *
 * This class relies heavily on using chainbase as a backing store.
 * Only one state_node can be active at a time.  This is enforced by
 * _current_node which is a weak_ptr.  New nodes will throw
 * NodeNotExpired exception if a
 */
class database_impl final
{
   public:
      database_impl() {}
      ~database_impl() { close(); }

      void open( const std::filesystem::path& p, std::function< void( state_node_ptr ) > init = nullptr );
      void close();

      void reset();
      state_node_ptr get_node_at_revision( uint64_t revision, const state_node_id& child ) const;
      state_node_ptr get_node( const state_node_id& node_id ) const;
      state_node_ptr create_writable_node( const state_node_id& parent_id, const state_node_id& new_id );
      void finalize_node( const state_node_id& node );
      void discard_node( const state_node_id& node, const std::unordered_set< state_node_id >& whitelist );
      void commit_node( const state_node_id& node );

      state_node_ptr get_head() const;
      std::vector< state_node_ptr > get_fork_heads() const;
      state_node_ptr get_root() const;

      bool is_open() const;

      std::filesystem::path                      _path;
      std::function< void( state_node_ptr ) >    _init_func = nullptr;

      state_multi_index_type                     _index;
      state_delta_ptr                            _head;
      std::map< state_node_id, state_delta_ptr > _fork_heads;
      state_delta_ptr                            _root;
};

void database_impl::reset()
{
   //
   // This method closes, wipes and re-opens the database.
   //
   // So the caller needs to be very careful to only call this method if deleting the database is desirable!
   //

   KOINOS_ASSERT( is_open(), database_not_open, "database is not open" );
   // Wipe and start over from empty database!
   _root->clear();
   close();
   open( _path, _init_func );
}

void database_impl::open( const std::filesystem::path& p, std::function< void( state_node_ptr ) > init )
{
   auto root = std::make_shared< state_node >();
   root->_state = std::make_shared< state_delta >( p );
   _init_func = init;

   if ( !root->revision() && root->_state->is_empty() && _init_func )
   {
      init( root );
   }
   root->_state->set_writable( false );
   _index.insert( root->_state );
   _root = root->_state;
   _head = root->_state;
   _fork_heads.insert_or_assign( _head->id(), _head );

   _path = p;
}

void database_impl::close()
{
   _fork_heads.clear();
   _root.reset();
   _head.reset();
   _index.clear();
}

state_node_ptr database_impl::get_node_at_revision( uint64_t revision, const state_node_id& child_id ) const
{
   KOINOS_ASSERT( is_open(), database_not_open, "database is not open" );
   KOINOS_ASSERT( revision >= _root->revision(), illegal_argument,
      "cannot ask for node with revision less than root. root rev: ${root}, requested: ${req}",
      ("root", _root->revision())("req", revision) );

   if( revision == _root->revision() )
      return get_root();

   auto child = get_node( child_id );
   if( !child )
      child = get_head();

   state_delta_ptr delta = child->_state;

   while( delta->revision() > revision )
   {
      delta = delta->parent();
   }

   auto node_itr = _index.find( delta->id() );

   KOINOS_ASSERT( node_itr != _index.end(), internal_error,
      "could not find state node associated with linked state_delta ${id}", ("id", delta->id() ) );

   auto node = std::make_shared< state_node >();
   node->_state = *node_itr;
   return node;
}

state_node_ptr database_impl::get_node( const state_node_id& node_id ) const
{
   KOINOS_ASSERT( is_open(), database_not_open, "database is not open" );

   auto node_itr = _index.find( node_id );

   if ( node_itr != _index.end() )
   {
      auto node = std::make_shared< state_node >();
      node->_state = *node_itr;
      return node;
   }

   return state_node_ptr();
}

state_node_ptr database_impl::create_writable_node( const state_node_id& parent_id, const state_node_id& new_id )
{
   KOINOS_ASSERT( is_open(), database_not_open, "database is not open" );

   auto parent_state = _index.find( parent_id );
   if( parent_state != _index.end() && !(*parent_state)->is_writable() )
   {
      auto node = std::make_shared< state_node >();
      node->_state = (*parent_state)->make_child( new_id );
      if( _index.insert( node->_state ).second )
         return node;
   }

   return state_node_ptr();
}

void database_impl::finalize_node( const state_node_id& node_id )
{
   KOINOS_ASSERT( is_open(), database_not_open, "database is not open" );
   auto node = get_node( node_id );
   KOINOS_ASSERT( node, illegal_argument, "node ${n} not found.", ("n", node_id) );

   node->_state->set_writable( false );

   if( node->revision() > _head->revision() )
   {
      _head = node->_state;
   }

   // When node is finalized, parent node needs to be removed from heads, if it exists.
   auto parent_itr = _fork_heads.find( node->parent_id() );
   if ( parent_itr != _fork_heads.end() )
   {
      _fork_heads.erase( parent_itr );
   }
   _fork_heads.insert_or_assign( node->id(), node->_state );
}

void database_impl::discard_node( const state_node_id& node_id, const std::unordered_set< state_node_id >& whitelist )
{
   KOINOS_ASSERT( is_open(), database_not_open, "database is not open" );
   auto node = get_node( node_id );

   if( !node ) return;

   KOINOS_ASSERT( node_id != _root->id(), illegal_argument, "cannot discard root node" );

   std::vector< state_node_id > remove_queue{ node_id };
   const auto& previdx = _index.template get< by_parent >();
   const auto head_id = _head->id();

   for( uint32_t i = 0; i < remove_queue.size(); ++i )
   {
      KOINOS_ASSERT( remove_queue[ i ] != head_id, cannot_discard, "cannot discard a node that would result in discarding of head" );

      auto previtr = previdx.lower_bound( remove_queue[ i ] );
      while ( previtr != previdx.end() && (*previtr)->parent_id() == remove_queue[ i ] )
      {
         // Do not remove nodes on the whitelist
         if ( whitelist.find( (*previtr)->id() ) == whitelist.end() )
         {
            remove_queue.push_back( (*previtr)->id() );
         }

         ++previtr;
      }

      // We may discard one or more fork heads when discarding a minority fork tree
      // For completeness, we'll check every node to see if it is a fork head
      auto head_itr = _fork_heads.find( remove_queue[ i ] );
      if ( head_itr != _fork_heads.end() )
      {
         _fork_heads.erase( head_itr );
      }
   }

   for( const auto& id : remove_queue )
   {
      auto itr = _index.find( id );
      if ( itr != _index.end() )
         _index.erase( itr );
   }

   // When node is discarded, if the parent node is not a parent of other nodes (no forks), add it to heads.
   auto fork_itr = previdx.find( node->parent_id() );
   if ( fork_itr == previdx.end() )
   {
      auto parent_itr = _index.find( node->parent_id() );
      KOINOS_ASSERT( parent_itr != _index.end(), internal_error, "discarded parent node not found in node index" );
      _fork_heads.insert_or_assign( (*parent_itr)->id(), *parent_itr );
   }
}

void database_impl::commit_node( const state_node_id& node_id )
{
   KOINOS_ASSERT( is_open(), database_not_open, "database is not open" );
   KOINOS_ASSERT( node_id != _root->id(), illegal_argument, "cannot commit root node, root node already committed" );
   auto node = get_node( node_id );
   KOINOS_ASSERT( node, illegal_argument, "node ${n} not found", ("n", node_id) );

   std::unordered_set< state_node_id > whitelist{ node->id() };

   auto old_root = _root;
   _root = node->_state;
   _index.modify( _index.find( node->id() ), []( state_delta_ptr& n ){ n->commit(); } );
   discard_node( old_root->id(), whitelist );
}

state_node_ptr database_impl::get_head() const
{
   KOINOS_ASSERT( is_open(), database_not_open, "database is not open" );
   auto head = std::make_shared< state_node >();
   head->_state = _head;
   return head;
}

std::vector< state_node_ptr > database_impl::get_fork_heads() const
{
   KOINOS_ASSERT( is_open(), database_not_open, "database is not open" );
   std::vector< state_node_ptr > fork_heads;
   fork_heads.reserve( _fork_heads.size() );

   for( auto& head : _fork_heads )
   {
      auto fork_head = std::make_shared< state_node >();
      fork_head->_state = head.second;
      fork_heads.push_back( fork_head );
   }

   return fork_heads;
}

state_node_ptr database_impl::get_root() const
{
   KOINOS_ASSERT( is_open(), database_not_open, "database is not open" );
   auto root = std::make_shared< state_node >();
   root->_state = _root;
   return root;
}

bool database_impl::is_open() const
{
   return (bool)_root && (bool)_head;
}

} // detail

state_node::state_node( detail::state_delta_ptr state ) : _state( state ) {}

const object_value* state_node::get_object( const object_space& space, const object_key& key ) const
{
   chain::database_key db_key;
   *db_key.mutable_space() = space;
   db_key.set_key( key );
   auto key_string = util::converter::as< std::string >( db_key );

   auto pobj = detail::merge_state( _state ).find( key_string );

   if( pobj != nullptr )
   {
      return pobj;
   }

   return nullptr;
}

std::pair< const object_value*, const object_key > state_node::get_next_object( const object_space& space, const object_key& key ) const
{
   chain::database_key db_key;
   *db_key.mutable_space() = space;
   db_key.set_key( key );
   auto key_string = util::converter::as< std::string >( db_key );

   auto state = detail::merge_state( _state );
   auto it = state.lower_bound( key_string );

   if ( it != state.end() && it.key() == key_string )
   {
      it++;
   }

   if( it != state.end() )
   {
      chain::database_key next_key = util::converter::to< chain::database_key >( it.key() );

      if ( next_key.space() == space )
      {
         return { &*it, next_key.key() };
      }
   }

   return { nullptr, detail::null_key };
}

std::pair< const object_value*, const object_key > state_node::get_prev_object( const object_space& space, const object_key& key ) const
{
   chain::database_key db_key;
   *db_key.mutable_space() = space;
   db_key.set_key( key );
   auto key_string = util::converter::as< std::string >( db_key );

   auto state = detail::merge_state( _state );
   auto it = state.lower_bound( key_string );

   if( it != state.begin() )
   {
      --it;
      chain::database_key next_key = util::converter::to< chain::database_key >( it.key() );

      if ( next_key.space() == space )
      {
         return { &*it, next_key.key() };
      }
   }

   return { nullptr, detail::null_key };
}

int32_t state_node::put_object( const object_space& space, const object_key& key, const object_value* val )
{
   KOINOS_ASSERT( _state->is_writable(), node_finalized, "cannot write to a finalized node" );

   chain::database_key db_key;
   *db_key.mutable_space() = space;
   db_key.set_key( key );
   auto key_string = util::converter::as< std::string >( db_key );

   auto pobj = detail::merge_state( _state ).find( key_string );

   int32_t bytes_used = 0;

   if ( pobj != nullptr )
   {
      bytes_used -= pobj->size();
   }

   bytes_used += val->size();
   _state->put( key_string, *val );

   return bytes_used;
}

void state_node::remove_object( const object_space& space, const object_key& key )
{
   KOINOS_ASSERT( _state->is_writable(), node_finalized, "cannot write to a finalized node" );

   chain::database_key db_key;
   *db_key.mutable_space() = space;
   db_key.set_key( key );

   _state->erase( util::converter::as< std::string >( db_key ) );
}

bool state_node::is_writable() const
{
   return _state->is_writable();
}

crypto::multihash state_node::get_merkle_root() const
{
   KOINOS_ASSERT( !is_writable(), koinos::exception, "cannot get the merkle root of a writable node" );
   return _state->get_merkle_root();
}

state_node_ptr state_node::create_anonymous_node()
{
   return std::make_shared< state_node >( _state->make_anonymous_child() );
}

const state_node_id& state_node::id() const
{
   return _state->id();
}

const state_node_id& state_node::parent_id() const
{
   return _state->parent_id();
}

uint64_t state_node::revision() const
{
   return _state->revision();
}

state_node_ptr state_node::get_parent() const
{
   auto parent_delta = _state->parent();
   if ( parent_delta )
   {
      return std::make_shared< state_node >( parent_delta );
   }

   return state_node_ptr();
}

void state_node::commit()
{
   KOINOS_ASSERT( _state->parent()->is_writable(), node_finalized, "cannot commit to a finalized node" );
   _state->squash();
   _state->reset();
}


database::database() : impl( new detail::database_impl() ) {}
database::~database() {}

void database::open( const std::filesystem::path& p, std::function< void( state_node_ptr ) > init )
{
   impl->open( p, init );
}

void database::close()
{
   impl->close();
}

void database::reset()
{
   impl->reset();
}

state_node_ptr database::get_node_at_revision( uint64_t revision, const state_node_id& child_id ) const
{
   return impl->get_node_at_revision( revision, child_id );
}

state_node_ptr database::get_node_at_revision( uint64_t revision ) const
{
   static const state_node_id null_id;
   return impl->get_node_at_revision( revision, null_id );
}

state_node_ptr database::get_node( const state_node_id& node_id ) const
{
   return impl->get_node( node_id );
}

state_node_ptr database::create_writable_node( const state_node_id& parent_id, const state_node_id& new_id )
{
   return impl->create_writable_node( parent_id, new_id );
}

void database::finalize_node( const state_node_id& node_id )
{
   impl->finalize_node( node_id );
}

void database::discard_node( const state_node_id& node_id )
{
   static const std::unordered_set< state_node_id > whitelist;
   impl->discard_node( node_id, whitelist );
}

void database::commit_node( const state_node_id& node_id )
{
   impl->commit_node( node_id );
}

state_node_ptr database::get_head() const
{
   return impl->get_head();
}

std::vector< state_node_ptr > database::get_fork_heads() const
{
   return impl->get_fork_heads();
}

state_node_ptr database::get_root() const
{
   return impl->get_root();
}

} // koinos::state_db
