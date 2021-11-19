#include <koinos/state_db/detail/state_delta.hpp>

namespace koinos::state_db::detail {

using backend_type = state_delta::backend_type;
using value_type   = state_delta::value_type;

state_delta::state_delta( std::shared_ptr< state_delta > parent, const state_node_id& id ) :
   _parent( parent ), _id( id )
{
   if ( _parent != nullptr )
   {
      _revision = _parent->_revision + 1;
   }

   _backend = std::make_shared< backends::map::map_backend >();
}

state_delta::state_delta( const std::filesystem::path& p )
{
   auto backend = std::make_shared< backends::rocksdb::rocksdb_backend >();
   backend->open( p );
   _backend = backend;
   _revision = backend->revision();
   _id = backend->id();
}

void state_delta::put( const key_type& k, const value_type& v )
{
   _backend->put( k, v );

   if ( !is_root() )
   {
      _modified_objects.insert( k );
   }
}

void state_delta::erase( const key_type& k )
{
   if ( _backend->find( k ) != _backend->end() )
   {
      _backend->erase( k );
   }

   _removed_objects.insert( k );
}

const value_type* state_delta::find( const key_type& key )
{
   auto itr = _backend->find( key );
   if ( itr != _backend->end() )
      return &*itr;

   if ( is_removed( key ) )
      return nullptr;

   return is_root() ? nullptr : _parent->find( key );
}

void state_delta::squash()
{
   if ( is_root() ) return;

   for ( key_type r_key : _removed_objects )
   {
      if ( _parent->_backend->find( r_key ) != _parent->_backend->end() )
      {
         _parent->_backend->erase( r_key );
      }
   }

   for ( auto mod_itr = _modified_objects.begin(); mod_itr != _modified_objects.end(); ++ mod_itr )
   {
      _parent->_backend->put( *mod_itr, *_backend->find( *mod_itr ) );
   }

   if ( !_parent->is_root() )
   {
      _parent->_removed_objects.merge( _removed_objects );

      // There is a corner case where if an object is created in parent and
      // modified here, then parent will show it as modified, when it is actually
      // new. I do not believe this will cause problems, but it is worth noting
      // it case it does.
      _parent->_modified_objects.merge( _modified_objects );
   }
}

void state_delta::squash( uint64_t revision )
{
   if ( revision < _revision && !is_root() )
   {
      squash();
      _parent->squash( revision );
   }
}

void state_delta::commit()
{
   KOINOS_ASSERT( !is_root(), internal_error, "Cannot commit root." );
   auto root = get_root();
   KOINOS_ASSERT( root, internal_error, "Could not get root" );

   squash( 0 );

   _backend = std::move( root->_backend );
   std::static_pointer_cast< backends::rocksdb::rocksdb_backend >( _backend )->set_revision( _revision );
   std::static_pointer_cast< backends::rocksdb::rocksdb_backend >( _backend )->set_id( _id );
   _modified_objects.clear();
   _removed_objects.clear();
   _parent.reset();
}

void state_delta::clear()
{
   _backend->clear();
   _modified_objects.clear();
   _removed_objects.clear();

   _revision = 0;
   _id = crypto::multihash::zero( crypto::multicodec::sha2_256 );
}

bool state_delta::is_modified( const key_type& k ) const
{
   return _modified_objects.find( k ) != _modified_objects.end()
      || _removed_objects.find( k ) != _removed_objects.end();
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

const std::shared_ptr< backend_type > state_delta::backend()const
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
   return _parent ? _parent->_id : null_id;
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
