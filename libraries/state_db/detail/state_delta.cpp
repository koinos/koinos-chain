#include <koinos/state_db/detail/state_delta.hpp>

#include <koinos/crypto/merkle_tree.hpp>

namespace koinos::state_db::detail {

using backend_type = state_delta::backend_type;
using value_type   = state_delta::value_type;

state_delta::state_delta( const std::filesystem::path& p )
{
   auto backend = std::make_shared< backends::rocksdb::rocksdb_backend >();
   backend->open( p );
   _revision = backend->revision();
   _id = backend->id();
   _merkle_root =  backend->merkle_root();
   _backend = backend;
}

void state_delta::put( const key_type& k, const value_type& v )
{
   _backend->put( k, v );
}

void state_delta::erase( const key_type& k )
{
   if ( find( k ) )
   {
      _backend->erase( k );
      _removed_objects.insert( k );
   }
}

const value_type* state_delta::find( const key_type& key ) const
{
   if ( auto val_ptr = _backend->get( key ); val_ptr )
      return val_ptr;

   if ( is_removed( key ) )
      return nullptr;

   return is_root() ? nullptr : _parent->find( key );
}

void state_delta::squash()
{
   if ( is_root() )
      return;

   // If an object is removed here and exists in the parent, it needs to only be removed in the parent
   // If an object is modified here, but removed in the parent, it needs to only be modified in the parent
   // These are O(m log n) operations. Because of this, squash should only be called from anonymouse state
   // nodes, whose modifications are much smaller
   for ( const key_type& r_key : _removed_objects )
   {
      _parent->_backend->erase( r_key );

      if ( !_parent->is_root() )
      {
         _parent->_removed_objects.insert( r_key );
      }
   }

   for ( auto itr = _backend->begin(); itr != _backend->end(); ++itr )
   {
      _parent->_backend->put( itr.key(), *itr );

      if ( !_parent->is_root() )
      {
         _parent->_removed_objects.erase( itr.key() );
      }
   }
}

void state_delta::commit_helper()
{
   if ( is_root() )
      return;

   _parent->commit_helper();

   std::static_pointer_cast< backends::rocksdb::rocksdb_backend >( _parent->_backend )->start_write_batch();

   for ( const key_type& r_key : _removed_objects )
   {
      _parent->_backend->erase( r_key );
   }

   for ( auto itr = _backend->begin(); itr != _backend->end(); ++itr )
   {
      _parent->_backend->put( itr.key(), *itr );
   }

   std::static_pointer_cast< backends::rocksdb::rocksdb_backend >( _parent->_backend )->end_write_batch();

   _backend = std::move( _parent->_backend );
}

void state_delta::commit()
{
   KOINOS_ASSERT( !is_root(), internal_error, "cannot commit root" );

   auto merkle_root = get_merkle_root();

   // As a side effect, get_root()->_backend has been moved to _backend
   commit_helper();

   std::static_pointer_cast< backends::rocksdb::rocksdb_backend >( _backend )->set_revision( _revision );
   std::static_pointer_cast< backends::rocksdb::rocksdb_backend >( _backend )->set_id( _id );
   std::static_pointer_cast< backends::rocksdb::rocksdb_backend >( _backend )->set_merkle_root( merkle_root );
   _removed_objects.clear();
   _parent.reset();
}

void state_delta::reset()
{
   _backend->clear();
   _removed_objects.clear();
}

void state_delta::clear()
{
   reset();

   _revision = 0;
   _id = crypto::multihash::zero( crypto::multicodec::sha2_256 );
}

bool state_delta::is_modified( const key_type& k ) const
{
   return _backend->get( k ) || _removed_objects.find( k ) != _removed_objects.end();
}

bool state_delta::is_removed( const key_type& k ) const
{
   return _removed_objects.find( k ) != _removed_objects.end();
}

bool state_delta::is_root() const
{
   return !_parent;
}

uint64_t state_delta::revision() const
{
   return _revision;
}

void state_delta::set_revision( uint64_t revision )
{
   _revision = revision;
   if ( is_root() )
   {
      std::static_pointer_cast< backends::rocksdb::rocksdb_backend >( _backend )->set_revision( revision );
   }
}

bool state_delta::is_writable() const
{
   return _writable;
}

void state_delta::set_writable( bool writable )
{
   _writable = writable;
}

crypto::multihash state_delta::get_merkle_root() const
{
   if ( !_merkle_root )
   {
      std::vector< std::string > object_keys;
      object_keys.reserve( _backend->size() + _removed_objects.size() );
      for ( auto itr = _backend->begin(); itr != _backend->end(); ++itr )
      {
         object_keys.push_back( itr.key() );
      }

      for ( const auto& removed : _removed_objects )
      {
         object_keys.push_back( removed );
      }

      std::sort(
         object_keys.begin(),
         object_keys.end()
      );

      std::vector< crypto::multihash > merkle_leafs;
      merkle_leafs.reserve( object_keys.size() * 2 );

      for ( const auto& key : object_keys )
      {
         merkle_leafs.emplace_back( crypto::hash( crypto::multicodec::sha2_256, key ) );
         auto val_ptr = _backend->get( key );
         merkle_leafs.emplace_back( crypto::hash( crypto::multicodec::sha2_256, val_ptr ? *val_ptr : std::string() ) );
      }

      _merkle_root = crypto::merkle_tree( crypto::multicodec::sha2_256, merkle_leafs ).root()->hash();
   }

   return *_merkle_root;
}

std::shared_ptr< state_delta > state_delta::make_anonymous_child()
{
   auto child = std::make_shared< state_delta >();
   child->_parent = shared_from_this();
   child->_id = _id;
   child->_revision = _revision;
   child->_backend = std::make_shared< backends::map::map_backend >();

   return child;
}

std::shared_ptr< state_delta > state_delta::make_child( const state_node_id& id )
{
   auto child = std::make_shared< state_delta >();
   child->_parent = shared_from_this();
   child->_id = id;
   child->_revision = _revision + 1;
   child->_backend = std::make_shared< backends::map::map_backend >();

   return child;
}

const std::shared_ptr< backend_type > state_delta::backend() const
{
   return _backend;
}

const state_node_id& state_delta::id() const
{
   return _id;
}

const state_node_id& state_delta::parent_id() const
{
   static const state_node_id null_id;
   if ( !_parent )
      return null_id;
   else if ( _parent->_id == _id )
      return _parent->parent_id();

   return _parent->id();
}

std::shared_ptr< state_delta > state_delta::parent() const
{
   return _parent;
}

bool state_delta::is_empty() const
{
   if ( _backend->size() )
      return false;
   else if ( _parent )
      return _parent->is_empty();

   return true;
}

std::shared_ptr< state_delta > state_delta::get_root()
{
   if ( !is_root() )
   {
      if ( _parent->is_root() )
         return _parent;
      else
         return _parent->get_root();
   }

   return std::shared_ptr< state_delta >();
}

} // koinos::state_db::detail
