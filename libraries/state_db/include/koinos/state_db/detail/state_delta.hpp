#pragma once
#include <koinos/state_db/backends/backend.hpp>
#include <koinos/state_db/backends/map/map_backend.hpp>
#include <koinos/state_db/backends/rocksdb/rocksdb_backend.hpp>
#include <koinos/state_db/state_db_types.hpp>

#include <koinos/crypto/multihash.hpp>

#include <any>
#include <filesystem>
#include <memory>
#include <unordered_set>

namespace koinos::state_db::detail {

   class state_delta
   {
      public:
         using backend_type  = backends::abstract_backend;
         using key_type      = backend_type::key_type;
         using value_type    = backend_type::value_type;

      private:
         std::shared_ptr< state_delta >  _parent;

         std::shared_ptr< backend_type > _backend;
         std::unordered_set< key_type >  _removed_objects;
         std::unordered_set< key_type >  _modified_objects;

         state_node_id                   _id;
         uint64_t                        _revision = 0;

      public:
         state_delta( std::shared_ptr< state_delta > parent, const state_node_id& id = state_node_id() ) :
            _parent( parent ), _id( id )
         {
            if ( _parent != nullptr )
            {
               _revision = _parent->_revision + 1;
            }

            _backend = std::make_shared< backends::map::map_backend >();
         }

         state_delta( const std::filesystem::path& p )
         {
            auto backend = std::make_shared< backends::rocksdb::rocksdb_backend >();
            backend->open( p );
            _backend = backend;
            _revision = backend->revision();
         }

         void put( const key_type& k, const value_type& v )
         {
            _backend->put( k, v );

            if ( !is_root() )
            {
               _modified_objects.insert( k );
            }
         }

         void erase( const key_type& k )
         {
            // If the key is in the current delta, just remove it, otherwise add it to the removed list
            if ( _backend->find( k ) != _backend->end() )
            {
               _backend->erase( k );
            }
            else
            {
               _removed_objects.insert( k );
            }
         }

         const value_type* find( const key_type& key )
         {
            auto itr = _backend->find( key );
            if ( itr != _backend->end() )
               return &*itr;

            const value_type* ptr = is_root() ? nullptr : _parent->find( key );

            if ( ptr != nullptr && is_removed( key ) )
               return nullptr;

            return ptr;
         }

         void squash()
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

         void squash( uint64_t revision )
         {
            if ( revision < _revision && !is_root() )
            {
               squash();
               _parent->squash( revision );
            }
         }

         void commit()
         {
            KOINOS_ASSERT( !is_root(), internal_error, "Cannot commit root." );
            auto root = get_root();
            KOINOS_ASSERT( root, internal_error, "Could not get root" );

            squash( 0 );

            _backend = std::move( root->_backend );
            std::static_pointer_cast< backends::rocksdb::rocksdb_backend >( _backend )->set_revision( _revision );
            _modified_objects.clear();
            _removed_objects.clear();
            _parent.reset();
         }

         void clear()
         {
            _backend->clear();
            _modified_objects.clear();
            _removed_objects.clear();

            _revision = 0;
            _id = crypto::multihash::zero( crypto::multicodec::sha2_256 );
         }

         bool is_modified( const key_type& k ) const
         {
            return _modified_objects.find( k ) != _modified_objects.end()
               || _removed_objects.find( k ) != _removed_objects.end();
         }

         bool is_removed( const key_type& k ) const
         {
            return _removed_objects.find( k ) != _removed_objects.end();
         }

         bool is_root() const
         {
            return !_parent;
         }

         uint64_t revision() const
         {
            return _revision;
         }

         void set_revision( uint64_t revision )
         {
            _revision = revision;
            if ( is_root() )
            {
               std::static_pointer_cast< backends::rocksdb::rocksdb_backend >( _backend )->set_revision( revision );
            }
         }

         const std::shared_ptr< backend_type > backend()const
         {
            return _backend;
         }

         const state_node_id& id() const
         {
            return _id;
         }

         const state_node_id& parent_id() const
         {
            static const state_node_id null_id;
            return _parent ? _parent->_id : null_id;
         }

         std::shared_ptr< state_delta > parent() const
         {
            return _parent;
         }

         bool is_empty() const
         {
            if ( _backend->size() )
               return false;
            else if ( _parent )
               return _parent->is_empty();

            return true;
         }

      private:
         std::shared_ptr< state_delta > get_root()
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
   };

} // koinos::state_db::detail
