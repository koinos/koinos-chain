#pragma once

#include <koinos/statedb/detail/state_delta.hpp>

#include <boost/container/deque.hpp>

#include <boost/iterator/zip_iterator.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

namespace koinos::statedb::detail {

   using namespace boost::multi_index;

   template< typename MultiIndexType, typename IndexedByType >
   class merge_iterator :
      public boost::bidirectional_iterator_helper<
         merge_iterator< MultiIndexType, IndexedByType >,
         typename MultiIndexType::value_type,
         std::size_t,
         const typename MultiIndexType::value_type*,
         const typename MultiIndexType::value_type& >
   {
      public:
         typedef typename MultiIndexType::value_type                                      value_type;
      private:
         typedef decltype( ((MultiIndexType*)nullptr)->template get< IndexedByType >() )  by_index_type;
         typedef typename by_index_type::iter_type                                        iter_type;
         typedef typename by_index_type::bmic_type::key_type                              key_type;
         typedef typename by_index_type::bmic_type::key_from_value                        key_from_value_type;
         typedef typename by_index_type::bmic_type::key_compare                           key_compare_type;
         typedef typename by_index_type::bmic_type::value_compare                         value_compare_type;
         typedef state_delta< MultiIndexType >                                            state_delta_type;
         typedef std::shared_ptr< state_delta_type >                                      state_delta_ptr;

         struct iterator_wrapper
         {
            iterator_wrapper( iter_type&& i, uint64_t r, const std::shared_ptr< MultiIndexType > idx ) :
               iter( std::move( i ) ),
               revision( r ),
               index( idx )
            {}

            iterator_wrapper( iterator_wrapper&& i ) :
               iter( std::move( i.iter ) ),
               revision( i.revision ),
               index( i.index )
            {}

            iterator_wrapper( const iterator_wrapper& i ) :
               iter( i.iter ),
               revision( i.revision ),
               index( i.index )
            {}

            iter_type                                 iter;
            uint64_t                                  revision;
            const std::shared_ptr< MultiIndexType >   index;

            const iterator_wrapper& self() const { return *this; }
            bool valid() const { return iter != index->template get< IndexedByType >().end(); }
         };

         /*
          * itertator_compare_less implements the composite key compare for iterator_wrapper
          * that would sort by iterator first, then by revision (greatest). Because we are
          * doing a comparision on the entire contained type, we can bypass the need for a
          * composite key altogether.
          */
         struct iterator_less_revision_greater_comparator
         {
            bool operator()( const iterator_wrapper& lhs, const iterator_wrapper& rhs )const
            {
               bool lh_valid = lhs.valid();
               bool rh_valid = rhs.valid();

               if( !lh_valid && !rh_valid ) return lhs.revision > rhs.revision;
               if( !lh_valid ) return false;
               if( rh_valid ) return true;

               // Indirection is normally a const method. However, because the rocksdb_iterator may
               // need to go to cache, internal state is updated and the operator is not const.
               // The value returned is always the same, even if internal state is updated, so this is safe.
               return value_compare_type()( *const_cast< iter_type& >(lhs.iter),
                                       *const_cast< iter_type& >(rhs.iter) );
            }
         };

         struct iterator_greater_revision_greater_comparator
         {
            bool operator()( const iterator_wrapper& lhs, const iterator_wrapper& rhs )const
            {
               bool lh_valid = lhs.valid();
               bool rh_valid = rhs.valid();

               if( !lh_valid && !rh_valid ) return lhs.revision > rhs.revision;
               if( !lh_valid ) return false;
               if( rh_valid ) return true;

               return value_compare_type()( *const_cast< iter_type& >(rhs.iter),
                                       *const_cast< iter_type& >(lhs.iter) );
            }
         };

         struct by_order_revision;
         struct by_reverse_order_revision;
         struct by_revision;


         typedef multi_index_container<
            iterator_wrapper,
            indexed_by<
               ordered_unique< tag< by_order_revision >,
                  composite_key< iterator_wrapper,
                     const_mem_fun< iterator_wrapper, const iterator_wrapper&, &iterator_wrapper::self >
                  >,
                  composite_key_compare< iterator_less_revision_greater_comparator >
               >,
               ordered_unique< tag< by_reverse_order_revision >,
                  composite_key< iterator_wrapper,
                     const_mem_fun< iterator_wrapper, const iterator_wrapper&, &iterator_wrapper::self >
                  >,
                  composite_key_compare< iterator_greater_revision_greater_comparator >
               >,
               ordered_unique< tag< by_revision >, member< iterator_wrapper, uint64_t, &iterator_wrapper::revision > >
            >
         > iter_revision_index_type;

         iter_revision_index_type                     _iter_rev_index;
         boost::container::deque< state_delta_ptr >   _delta_deque;

      public:
         template< typename Initializer >
         merge_iterator( state_delta_ptr head, Initializer&& init )
         {
            KOINOS_ASSERT( head, internal_error, "Cannot create a merge iterator on an null delta.", () );
            auto current_delta = head;

            do
            {
               _delta_deque.push_front( current_delta );

               const auto& by_index = current_delta->indices()->template get< IndexedByType >();
               iterator_wrapper undo_itr(
                  std::move( init( by_index ) ),
                  current_delta->revision(),
                  current_delta->indices() );

               _iter_rev_index.emplace( std::move( undo_itr ) );

               current_delta = current_delta->parent();
            } while( current_delta );

            resolve_conflicts();
         }

         merge_iterator( const boost::container::deque< state_delta_ptr >& deque ) :
            _delta_deque( deque )
         {}

         merge_iterator( const merge_iterator& other ) :
            _iter_rev_index( other._iter_rev_index ),
            _delta_deque( other._delta_deque )
         {}

         merge_iterator( merge_iterator&& other ) :
            _iter_rev_index( std::move( other._iter_rev_index ) ),
            _delta_deque( other._delta_deque )
         {}

         merge_iterator() {}

         bool operator ==( const merge_iterator& other )const
         {
            // If both iterators are empty, they are true.
            // But we use empty merge iterators as an optimization for an end itertor.
            // So if one is empty, and the other is all end iterators, they are also equal.
            if( _iter_rev_index.size() == 0 && other._iter_rev_index.size() == 0 ) return true;
            else if( _iter_rev_index.size() ) return other.is_end();
            else if( other._iter_rev_index.size() ) return is_end();

            auto my_begin = _iter_rev_index.begin();
            auto other_begin = other._iter_rev_index.begin();

            if( !my_begin->valid() && !other_begin->valid() ) return true;
            if( !my_begin->valid() || !other_begin->valid() ) return false;
            if( my_begin->revision != other_begin->revision ) return false;

            return my_begin->iter == other_begin->iter;
         }

         merge_iterator& operator ++()
         {
            auto first_itr = _iter_rev_index.begin();

            if( first_itr->valid() )
            {
               _iter_rev_index.modify( first_itr, []( iterator_wrapper& i ){ ++(i.iter); } );
               resolve_conflicts();
            }

            return *this;
         }

         merge_iterator operator ++(int)const
         {
            return ++(merge_iterator( *this ));
         }

         merge_iterator& operator --()
         {
            const auto& order_idx = _iter_rev_index.template get< by_order_revision >();

            auto head_itr = order_idx.begin();
            // composite keys do not have default initializers, so we need to store them as a pointer
            std::unique_ptr< key_type > head_key;

            key_from_value_type key_from_value;
            key_compare_type    key_compare;

            if( head_itr->valid() )
            {
               head_key = std::make_unique< key_type >( key_from_value( *(head_itr->iter) ) );
            }

            /* We are grabbing the current head value.
             * Then iterate over all other iterators and rewind them until they have a value less
             * than the current value. One of those values is what we want to decrement to.
             */
            const auto& rev_idx = _iter_rev_index.template get< by_revision >();
            for( auto rev_itr = rev_idx.begin(); rev_itr != rev_idx.end(); ++rev_itr )
            {
               // Only decrement iterators that have modified objects
               if( rev_itr->index->size() )
               {
                  auto begin = rev_itr->index->template get< IndexedByType >().begin();

                  if( !head_key )
                  {
                     // If there was no valid key, then bring back each iterator once, it is gauranteed to be less than the
                     // current value (end()).
                     _iter_rev_index.modify( _iter_rev_index.iterator_to( *rev_itr ), [&]( iterator_wrapper& i ){ --(i.iter); } );
                  }
                  else
                  {
                     // Do an initial decrement if the iterator currently points to end()
                     if( !rev_itr->valid() )
                     {
                        _iter_rev_index.modify( _iter_rev_index.iterator_to( *rev_itr ), [&]( iterator_wrapper& i ){ --(i.iter); } );
                     }

                     // Decrement back to the first key that is less than the head key
                     while( !key_compare( key_from_value( *(rev_itr->iter) ), *head_key ) && rev_itr->iter != begin )
                     {
                        _iter_rev_index.modify( _iter_rev_index.iterator_to( *rev_itr ), [&]( iterator_wrapper& i ){ --(i.iter); } );
                     }
                  }

                  // The key at this point is guaranteed to be less than the head key (or at begin() and greator), but it
                  // might have been modified in a later index. We need to continue decrementing until we have a valid key.
                  bool dirty = true;

                  while( dirty && rev_itr->valid() && rev_itr->iter != begin )
                  {
                     dirty = is_dirty( rev_itr );

                     if( dirty )
                     {
                        _iter_rev_index.modify( _iter_rev_index.iterator_to( *(rev_itr) ), [](iterator_wrapper& i ){ --(i.iter); } );
                     }
                  }
               }
            }

            const auto& rev_order_idx = _iter_rev_index.template get< by_reverse_order_revision >();
            auto least_itr = rev_order_idx.begin();

            if( _delta_deque.size() > 1 )
            {
               // This next bit works in two modes.
               // Some indices may not have had a value less than the previous head, so they will show up first,
               // we need to increment through those values until we get the the new valid least value.
               if( head_key )
               {
                  while( least_itr != rev_order_idx.end() && least_itr->valid()
                     && ( is_dirty( least_itr ) || !key_compare( key_from_value( *(least_itr->iter) ), *head_key ) ) )
                  {
                     ++least_itr;
                  }
               }

               // Now least_itr points to the new least value, unless it is end()
               if( least_itr != rev_order_idx.end() )
               {
                  ++least_itr;
               }

               // Now least_itr points to the next value. All of these are too much less, but are guaranteed to be valid.
               // All values in this indices one past are gauranteed to be greater than the new least, or invalid by
               // modification. We can increment all of them once, and then call resolve_conflicts for the new least value
               // to become the head.
               while( least_itr != rev_order_idx.end() && least_itr->valid() )
               {
                  _iter_rev_index.modify( _iter_rev_index.iterator_to( *(least_itr--) ), [](iterator_wrapper& i ){ ++(i.iter); } );
                  ++least_itr;
               }

               resolve_conflicts();
            }

            return *this;
         }

         merge_iterator operator --(int)const
         {
            return --(merge_iterator( *this ));
         }

         const value_type& operator*()const
         {
            return _iter_rev_index.begin()->iter.operator *();
         }

         const value_type* operator->()const
         {
            return _iter_rev_index.begin()->iter.operator ->();
         }

         merge_iterator& operator =( const merge_iterator& other )
         {
            KOINOS_ASSERT( _delta_deque.size(), internal_error, "Merge iterator is unexpectedly empty" );
            KOINOS_ASSERT( _delta_deque.size() == other._delta_deque.size(), internal_error, "Cannot assign iterators with different delta deques.", () );
            KOINOS_ASSERT( _delta_deque.begin()->id() == _delta_deque.begin()->id(), internal_error, "Cannot assign merge iterators with different roots", () );
            KOINOS_ASSERT( _delta_deque.rbegin()->id() == _delta_deque.rbegin()->id(), internal_error, "Cannot assign merge iterators with different heads", () );

            _iter_rev_index = other._iter_rev_index;

            return *this;
         }

         merge_iterator& operator =( merge_iterator&& other )
         {
            KOINOS_ASSERT( _delta_deque.size(), internal_error, "Merge iterator is unexpectedly empty" );
            KOINOS_ASSERT( _delta_deque.size() == other._delta_deque.size(), internal_error, "Cannot assign iterators with different delta deques.", () );
            KOINOS_ASSERT( _delta_deque.begin()->id() == _delta_deque.begin()->id(), internal_error, "Cannot assign merge iterators with different roots", () );
            KOINOS_ASSERT( _delta_deque.rbegin()->id() == _delta_deque.rbegin()->id(), internal_error, "Cannot assign merge iterators with different heads", () );

            _iter_rev_index = std::move( other._iter_rev_index );

            return *this;
         }

      private:
         template< typename IterType >
         bool is_dirty( IterType itr )
         {
            bool dirty = false;

            for( int i = _delta_deque.size() - 1; itr->revision < _delta_deque[i]->revision() && !dirty; --i )
            {
               dirty = _delta_deque[i]->is_modified( itr->iter->id );
            }

            return dirty;
         }

         void resolve_conflicts()
         {
            auto first_itr = _iter_rev_index.begin();
            bool dirty = true;

            while( dirty && first_itr->valid() )
            {
               dirty = is_dirty( first_itr );

               if( dirty )
               {
                  _iter_rev_index.modify( first_itr, [](iterator_wrapper& i ){ ++(i.iter); } );
               }

               first_itr = _iter_rev_index.begin();
            }
         }

         bool is_end() const
         {
            return std::all_of( _iter_rev_index.begin(), _iter_rev_index.end(),
               []( auto& i ){ return !i.valid(); } );
         }
   };

   template< typename MultiIndexType, typename IndexedByType >
   class merge_index
   {
      public:
         typedef MultiIndexType                                                        index_type;
         typedef state_delta< index_type >                                             state_delta_type;
         typedef decltype( ((index_type*)nullptr)->template get< IndexedByType >() )   by_index_type;
         typedef typename index_type::value_type                                       value_type;
         typedef merge_iterator< index_type, IndexedByType >                           iterator_type;

         std::shared_ptr< state_delta_type > _head;

         merge_index( std::shared_ptr< state_delta_type > head ) :
            _head( head )
         {}

         template< typename CompatibleKey >
         iterator_type lower_bound( CompatibleKey&& key ) const
         {
            return iterator_type( _head, [&]( by_index_type& idx )
            {
               return idx.lower_bound( key );
            });
         }

         template< typename CompatibleKey >
         iterator_type upper_bound( CompatibleKey&& key ) const
         {
            return iterator_type( _head, [&]( by_index_type& idx )
            {
               return idx.upper_bound( key );
            });
         }

         template< typename CompatibleKey >
         std::pair< iterator_type, iterator_type > equal_range( CompatibleKey&& key ) const
         {
            return std::make_pair< iterator_type, iterator_type >(
               lower_bound( key ), upper_bound( key ) );
         }

         iterator_type begin() const
         {
            return iterator_type( _head, [&]( by_index_type& idx )
            {
               return idx.begin();
            });
         }

         iterator_type end() const
         {
            return iterator_type();
         }

         template< typename CompatibleKey >
         const value_type* find( CompatibleKey&& key ) const
         {
            return _head->template find< IndexedByType >( key );
         }

         iterator_type iterator_to( const value_type& v ) const
         {
            return iterator_type( _head, [&]( by_index_type& idx )
            {
               auto itr = idx.iterator_to( v );
               return itr != idx.end() ? itr : idx.upper_bound( v );
            });
         }
   };

} // koinos::statedb::detail
