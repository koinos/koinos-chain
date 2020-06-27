#pragma once
#include <koinos/statedb/statedb_types.hpp>
#include <koinos/statedb/detail/uniqueness_validator.hpp>

#include <koinos/crypto/multihash.hpp>

#include <boost/any.hpp>
#include <boost/filesystem.hpp>

#include <memory>

const std::vector< uint8_t > ID_KEY { 'D','E','L','T','A','_','I','D' };

namespace koinos::statedb::detail {

   using boost::container::flat_set;

   template< typename MultiIndexType >
   class state_delta
   {
      private:
         typedef MultiIndexType                    index_type;
         typedef typename index_type::value_type   value_type;
         typedef typename value_type::id_type      id_type;
         typedef typename index_type::iter_type    iter_type;

         std::shared_ptr< state_delta >            _parent;

         std::shared_ptr< index_type >             _indices;
         flat_set< id_type >                       _removed_objects;
         flat_set< id_type >                       _modified_objects;
         id_type                                   _next_object_id = 0;

         state_node_id                             _id;
         uint64_t                                  _revision = 0;

      public:
         state_delta( std::shared_ptr< state_delta > parent, const state_node_id& id ) :
            _parent( parent ), _id( id )
         {
            if( _parent != nullptr )
            {
               _revision = _parent->_revision + 1;
               _next_object_id = _parent->_next_object_id;
            }

            _indices = std::make_shared< index_type >( index_type::type_enum::bmic );
         }

         state_delta( const boost::filesystem::path& p, const boost::any& o )
         {
            _indices = std::make_shared< index_type >( index_type::type_enum::mira );
            _indices->open( p, o );
            _next_object_id = _indices->next_id();
            _revision = _indices->revision();
            if( !_indices->get_metadata( ID_KEY, _id ) )
            {
               crypto::zero_hash( _id, CRYPTO_SHA2_256_ID );
               _indices->put_metadata( ID_KEY, _id );
            }
         }

         template< typename Constructor >
         const std::pair< iter_type, bool > emplace( Constructor&& c )
         {
            auto constructor = [&]( value_type& v )
            {
               v.id = _next_object_id;
               c( v );
            };

            value_type new_obj;

            constructor( new_obj );
            flat_set< id_type > ids;

            if( !is_unique( new_obj ) )
               return std::make_pair( _indices->end(), false );

            auto emplace_result = _indices->emplace( [&]( value_type& v )
            {
               v = std::move( new_obj );
            });

            if( emplace_result.second )
            {
               ++_next_object_id;

               if( is_root() )
               {
                  _indices->set_next_id( _next_object_id );
               }
               else
               {
                  _modified_objects.insert( new_obj.id );
               }
            }

            return emplace_result;
         }

         template< typename Modifier >
         bool modify( const value_type& obj, Modifier&& m )
         {
            if( is_root() )
               return _indices->modify( _indices->iterator_to( obj ), m );

            value_type mod_obj = obj;
            m( mod_obj );
            flat_set< id_type > ids;

            if( !is_unique( obj ) )
               return false;

            if( _modified_objects.find( obj.id ) != _modified_objects.end() )
            {
               _indices->modify( _indices->iterator_to( obj ), m );
            }
            else
            {
               _indices->emplace( [&]( value_type& v )
               {
                  v = std::move( mod_obj );
                  m( v );
               });

               _modified_objects.insert( obj.id );
            }

            return true;
         }

         /**
          * It is the caller's responsiblility to check that obj exists.
          *
          * If it does not, the id will be added to removed_objects regardless
          * of its previous existence.
          */
         void erase( const value_type& obj )
         {
            if( !is_root() )
               _removed_objects.insert( obj.id );

            auto itr = _indices->find( obj.id );

            if( itr != _indices->end() )
               _indices->erase( itr );
         }

         template< typename IndexedByType, typename CompatibleKey >
         const value_type* find( CompatibleKey& key )
         {
            const auto& by_index = _indices->template get< IndexedByType >();
            auto itr = by_index.find( key );
            if( itr != by_index.end() ) return &*itr;

            const auto* ptr = is_root() ? nullptr : _parent->template find< IndexedByType >( key );

            if( ptr != nullptr && is_removed( ptr->id ) ) return nullptr;

            return ptr;
         }

         template< typename IndexedByType, typename CompatibleKey >
         const value_type* find( CompatibleKey&& key )
         {
            return find< IndexedByType >( key );
         }

         void squash()
         {
            if( is_root() ) return;

            for( id_type r_id : _removed_objects )
            {
               auto r_itr = _parent->_indices->find( r_id );
               if( r_itr != _parent->_indices->end() )
               {
                  _parent->_indices->erase( r_itr );
               }
            }

            for( auto mod_itr = _indices->begin(); mod_itr != _indices->end(); ++mod_itr )
            {
               auto itr = _parent->_indices->find( mod_itr->id );
               if( itr == _parent->_indices->end() )
               {
                  _parent->_indices->emplace( [&]( value_type& v )
                  {
                     v = *mod_itr;
                  });
               }
               else
               {
                  _parent->_indices->modify( itr, [&]( value_type& v )
                  {
                     v = *mod_itr;
                  });
               }
            }

            _parent->_next_object_id = _next_object_id;

            if( _parent->is_root() )
            {
               _parent->_indices->set_next_id( _next_object_id );
            }
            else
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
            if( revision < _revision && !is_root() )
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

            _indices = std::move( root->_indices );
            _indices->set_next_id( _next_object_id );
            _indices->set_revision( _revision );
            _indices->put_metadata( ID_KEY, _id );
            _modified_objects.clear();
            _removed_objects.clear();
            _parent.reset();
         }

         void clear()
         {
            _indices->clear();
            _modified_objects.clear();
            _removed_objects.clear();

            if( is_root() )
               _next_object_id = 0;
            else
               _next_object_id = _parent->_next_object_id;
         }

         void wipe( const boost::filesystem::path& dir )
         {
            _indices->wipe( dir );
            _modified_objects.clear();
            _removed_objects.clear();

            if( is_root() )
               _next_object_id = 0;
            else
               _next_object_id = _parent->_next_object_id;
         }

         void flush()
         {
            _indices->flush();
         }

         size_t get_cache_usage() const
         {
            return _indices->get_cache_usage();
         }

         size_t get_cache_size() const
         {
            return _indices->get_cache_size();
         }

         void dump_lb_call_counts()
         {
            _indices->dump_lb_call_counts();
         }

         void trim_cache()
         {
            _indices->trim_cache();
         }

         bool is_modified( const id_type& id ) const
         {
            return _modified_objects.find( id ) != _modified_objects.end()
               || _removed_objects.find( id ) != _removed_objects.end();
         }

         bool is_removed( const id_type& id ) const
         {
            return _removed_objects.find( id ) != _removed_objects.end();
         }

         bool is_root() const
         {
            return !_parent;
         }

         id_type next_object_id() const
         {
            return _next_object_id;
         }

         uint64_t revision() const
         {
            return _revision;
         }

         void set_revision( uint64_t revision )
         {
            _revision = revision;
            if( is_root() )
               _indices->set_revision( revision );
         }

         const std::shared_ptr< index_type > indices()const
         {
            return _indices;
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

      private:
         bool is_unique( const value_type& v ) const
         {
            flat_set< id_type > conflict_set;
            check_uniqueness( v, conflict_set );

            if( conflict_set.size() == 0 ) return true;
            if( conflict_set.size() > 1 ) return false;

            // If there are conflicts other than the current object then it is not unique
            return conflict_set.find( v.id ) != conflict_set.end();
         }

         /*
          * This method is a little tricky.
          *
          * Boost Multi Index Containers enforce uniqueness by trying to insert
          * an object and failing. That technique does not work here for two
          * reasons.
          *
          * 1. We are inserting on the head delta, which may or may not contain
          * the objects that the new value would conflict with. So a succesful
          * insertion tells us nothing of previous states.
          *
          * 2. We do not want to write to old deltas. This violates the
          * finality of the state and destroys all semblence of thread safety.
          *
          * We use forward recursion to iterate on all deltas, oldest to newest.
          *
          * At each delta we apply the following rules:
          * - If an id from the conflict set was modified in the current delta,
          *   remove it from the conflict set.
          * - Check against all updated values, adding them to the conflict set
          *   if a conflict is found.
          *
          * The expected result at the end of the recursion is a set containing
          * a single conflict ID, matching the ID of the value. This is not a
          * problem as a value cannot conflict against itself.
          */
         void check_uniqueness( const value_type& v, flat_set< id_type >& conflict_set ) const
         {
            if( !is_root() )
            {
               _parent->check_uniqueness( v, conflict_set );

               for( auto itr = conflict_set.begin(); itr != conflict_set.end(); ++itr )
               {
                  if( is_modified( *itr ) )
                  {
                     itr = conflict_set.erase( itr );
                     if( itr == conflict_set.end() ) break;
                  }
               }
            }

            find_uniqueness_conflicts( *_indices, v, conflict_set );
         }

         std::shared_ptr< state_delta > get_root()
         {
            if( !is_root() )
            {
               if( _parent->is_root() )
                  return _parent;
               else
                  return _parent->get_root();
            }

            return std::shared_ptr< state_delta >();
         }
   };

} // koinos::statedb::detail

#undef ID_KEY
