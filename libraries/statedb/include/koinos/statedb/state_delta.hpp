#pragma once
#include <koinos/statedb/statedb_types.hpp>
#include <koinos/statedb/uniqueness_validator.hpp>

#include <boost/any.hpp>
#include <boost/filesystem.hpp>

#include <memory>

#define STATE_ID_KEY "state_id"

namespace koinos::statedb {

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

         uint256_t                                 _state_id = 0;
         uint64_t                                  _state_revision = 0;
         bool                                      _is_writable = true;

      public:
         state_delta( std::shared_ptr< state_delta > parent, uint256_t& id ) :
            _parent( parent )
         {
            if( _parent != nullptr )
            {
               _state_revision = _parent->_state_revision + 1;
               _next_object_id = _parent->_next_object_id;
            }

            _indices = std::make_shared< index_type >( index_type::type_enum::bmic );
         }

         state_delta( const boost::filesystem::path& p, const boost::any& o )
         {
            _indices = std::make_shared< index_type >( index_type::type_enum::mira );
            _indices->open( p, o );
            _next_object_id = _indices->next_id();
            _state_revision = _indices->revision();
            _indices->get_metadata( STATE_ID_KEY, _state_id );
         }

         template< typename Constructor >
         const std::pair< iter_type, bool > emplace( Constructor&& c )
         {
            KOINOS_ASSERT( _is_writable, node_finalized, "Cannot modify a finalized node", () );
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
               ++_next_object_id;

            if( is_root() )
               _indices->set_next_id( _next_object_id );

            return emplace_result;
         }

         template< typename Modifier >
         bool modify( const value_type& obj, Modifier&& m )
         {
            KOINOS_ASSERT( _is_writable, node_finalized, "Cannot modify a finalized node", () );
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

         void erase( const value_type& obj )
         {
            KOINOS_ASSERT( _is_writable, node_finalized, "Cannot modify a finalized node", () );
            auto itr = _indices->find( obj.id );

            if( itr != _indices->end() )
               _indices->erase( itr );

            if( !is_root() )
               _removed_objects.insert( obj.id );
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

         const value_type* find( id_type& key )
         {
            auto itr = _indices->find( key );
            if( itr != _indices->end() ) return &*itr;

            const auto* ptr = is_root() ? nullptr : _parent->find( key );

            if( ptr != nullptr && is_removed( ptr->id ) ) return nullptr;

            return ptr;
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

         void squash( int64_t revision )
         {
            if( revision < _state_revision && !is_root() )
            {
               squash();
               _parent->squash( revision );
            }
         }

         void commit()
         {
            squash( 0 );
            auto root = get_root();

            _indices = std::move( root->_indices );
            _indices->set_next_id( _next_object_id );
            _indices->set_revision( _state_revision );
            _indices->put_metadata( STATE_ID_KEY, _state_id );
            _modified_objects.clear();
            _removed_objects.clear();
            _parent.reset();
         }

         void commit( int64_t revision )
         {
            if( revision > _state_revision && !is_root() )
            {
               _parent->commit( revision );
            }
            else if( revision == _state_revision )
            {
               commit();
            }
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
            return _state_revision;
         }

         void set_state_revision( int64_t revision )
         {
            _state_revision = revision;
            if( is_root() )
               _indices->set_revision( revision );
         }

         void finalize()
         {
            _is_writable = false;
         }

         bool is_writable()
         {
            return _is_writable;
         }

         const std::shared_ptr< index_type > indices()const
         {
            return _indices;
         }

         size_t size() const
         {
            size_t s = 0;
            if( !is_root() )
               s = _parent->size();

            s += indices()->size() - _modified_objects.size();
            return s;
         }

         uint256_t state_id() const
         {
            return _state_id;
         }

         uint256_t parent_id() const
         {
            return _parent ? _parent->_state_id : 0;
         }

         std::shared_ptr< state_delta > parent() const
         {
            return _parent;
         }

      private:
         bool is_unique( const value_type& v ) const
         {
            flat_set< id_type > ids;
            check_uniqueness( v, ids );

            if( ids.size() == 0 ) return true;
            if( ids.size() > 1 ) return false;

            // If there are conflicts other than the current object then it is not unique
            return ids.find( v.id ) != ids.end();
         }

         void check_uniqueness( const value_type& v, flat_set< id_type >& ids ) const
         {
            if( !is_root() )
            {
               _parent->check_uniqueness( v, ids );

               for( auto itr = ids.begin(); itr != ids.end(); ++itr )
               {
                  if( is_modified( *itr ) )
                  {
                     itr = ids.erase( itr );
                     if( itr == ids.end() ) break;
                  }
               }
            }

            #pragma message "TODO: Remove namespacing once chainbase is not included"
            koinos::statedb::find_uniqueness_conflicts( *_indices, v, ids );
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

} // koinos::statedb

#undef STATE_ID_KEY

//candidate function [with MultiIndexType = mira::multi_index_adapter<koinos::statedb::state_object, boost::multi_index::indexed_by<mira::multi_index::ordered_unique<boost::multi_index::tag<koinos::statedb::by_id, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na>, mira::multi_index::member<koinos::statedb::state_object, chainbase::oid<koinos::statedb::state_object>, &koinos::statedb::state_object::id>, mpl_::na>, mira::multi_index::ordered_unique<boost::multi_index::tag<koinos::statedb::by_key, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na>, mira::multi_index::composite_key<koinos::statedb::state_object, mira::multi_index::member<koinos::statedb::state_object, boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<256, 256, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>, boost::multiprecision::et_off>, &koinos::statedb::state_object::space>, mira::multi_index::member<koinos::statedb::state_object, boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<256, 256, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>, boost::multiprecision::et_off>, &koinos::statedb::state_object::key>, boost::tuples::null_type, boost::tuples::null_type, boost::tuples::null_type, boost::tuples::null_type, boost::tuples::null_type, boost::tuples::null_type, boost::tuples::null_type, boost::tuples::null_type>, mpl_::na>, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na>, std::__1::allocator<koinos::statedb::state_object> >]
//candidate function [with MultiIndexType = mira::multi_index_adapter<koinos::statedb::state_object, boost::multi_index::indexed_by<mira::multi_index::ordered_unique<boost::multi_index::tag<koinos::statedb::by_id, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na>, mira::multi_index::member<koinos::statedb::state_object, chainbase::oid<koinos::statedb::state_object>, &koinos::statedb::state_object::id>, mpl_::na>, mira::multi_index::ordered_unique<boost::multi_index::tag<koinos::statedb::by_key, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na>, mira::multi_index::composite_key<koinos::statedb::state_object, mira::multi_index::member<koinos::statedb::state_object, boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<256, 256, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>, boost::multiprecision::et_off>, &koinos::statedb::state_object::space>, mira::multi_index::member<koinos::statedb::state_object, boost::multiprecision::number<boost::multiprecision::backends::cpp_int_backend<256, 256, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>, boost::multiprecision::et_off>, &koinos::statedb::state_object::key>, boost::tuples::null_type, boost::tuples::null_type, boost::tuples::null_type, boost::tuples::null_type, boost::tuples::null_type, boost::tuples::null_type, boost::tuples::null_type, boost::tuples::null_type>, mpl_::na>, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na, mpl_::na>, std::__1::allocator<koinos::statedb::state_object> >]