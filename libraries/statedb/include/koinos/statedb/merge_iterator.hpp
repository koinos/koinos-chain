#pragma once
#include <koinos/statedb/state_delta.hpp>

#include <boost/container/deque.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

namespace koinos::statedb {

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
         typedef typename value_type::id_type                                             id_type;
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
            iterator_wrapper( iter_type&& i, int64_t r, const std::shared_ptr< MultiIndexType > idx ) :
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
            int64_t                                   revision;
            const std::shared_ptr< MultiIndexType >   index;

            const iterator_wrapper& self() const { return *this; }
            bool valid() const { return iter != index->template get< IndexedByType >().end(); }
         };

         struct iterator_compare_less
         {
            bool operator()( const iterator_wrapper& lhs, const iterator_wrapper& rhs )const
            {
               if( !lhs.valid() )
               {
                  return false;
               }
               else if( !rhs.valid() )
               {
                  return true;
               }

               // Indirection is normally a const method. However, because the rocksdb_iterator may
               // need to go to cache, internal state is updated and the operator is not const.
               // The value returned is always the same, even if internal state is updated, so this is safe.
               return value_compare_type()( *const_cast< iter_type& >(lhs.iter),
                                       *const_cast< iter_type& >(rhs.iter) );
            }
         };

         struct iterator_compare_greater
         {
            bool operator()( const iterator_wrapper& lhs, const iterator_wrapper& rhs )const
            {
               if( !lhs.valid() )
               {
                  return false;
               }
               else if( !rhs.valid() )
               {
                  return true;
               }

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
                     const_mem_fun< iterator_wrapper, const iterator_wrapper&, &iterator_wrapper::self >,
                     member< iterator_wrapper, int64_t, &iterator_wrapper::revision >
                  >,
                  composite_key_compare< iterator_compare_less, std::greater< int64_t > >
               >,
               ordered_unique< tag< by_reverse_order_revision >,
                  composite_key< iterator_wrapper,
                     const_mem_fun< iterator_wrapper, const iterator_wrapper&, &iterator_wrapper::self >,
                     member< iterator_wrapper, int64_t, &iterator_wrapper::revision >
                  >,
                  composite_key_compare< iterator_compare_greater, std::greater< int64_t > >
               >,
               ordered_unique< tag< by_revision >, member< iterator_wrapper, int64_t, &iterator_wrapper::revision > >
            >
         > iter_revision_index_type;

         iter_revision_index_type                           iter_revision_index;
         const boost::container::deque< state_delta_ptr >&   undo_deque;
         int64_t                                            base_revision = 0;

      public:
         template< typename Initializer >
         merge_iterator( const boost::container::deque< state_delta_ptr >& deque, Initializer&& init ) :
            undo_deque( deque )
         {
            for( const auto& undo : undo_deque )
            {
               const auto& by_index = undo->indices()->template get< IndexedByType >();
               iterator_wrapper undo_itr(
                  std::move( init( by_index ) ),
                  undo->revision(),
                  undo->indices() );

               iter_revision_index.emplace( std::move( undo_itr ) );
            }

            resolve_conflicts();
         }

         merge_iterator( const boost::container::deque< state_delta_ptr >& deque ) :
            undo_deque( deque )
         {}

         merge_iterator( const merge_iterator& other ) :
            iter_revision_index( other.iter_revision_index ),
            undo_deque( other.undo_deque ),
            base_revision( other.base_revision )
         {}

         merge_iterator( merge_iterator&& other ) :
            iter_revision_index( std::move( other.iter_revision_index ) ),
            undo_deque( other.undo_deque ),
            base_revision( other.base_revision )
         {}

         bool operator ==( const merge_iterator& other )const
         {
            if( iter_revision_index.size() == 0 && other.iter_revision_index.size() == 0 ) return true;
            auto my_begin = iter_revision_index.begin();
            auto other_begin = other.iter_revision_index.begin();

            if( !my_begin->valid() && !other_begin->valid() ) return true;
            if( !my_begin->valid() || !other_begin->valid() ) return false;
            if( my_begin->revision != other_begin->revision ) return false;

            return my_begin->iter == other_begin->iter;
         }

         merge_iterator& operator ++()
         {
            auto first_itr = iter_revision_index.begin();

            if( first_itr->valid() )
            {
               iter_revision_index.modify( first_itr, []( iterator_wrapper& i ){ ++(i.iter); } );
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
            const auto& order_idx = iter_revision_index.template get< by_order_revision >();

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
            const auto& rev_idx = iter_revision_index.template get< by_revision >();
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
                     iter_revision_index.modify( iter_revision_index.iterator_to( *rev_itr ), [&]( iterator_wrapper& i ){ --(i.iter); } );
                  }
                  else
                  {
                     // Do an initial decrement if the iterator currently points to end()
                     if( !rev_itr->valid() )
                     {
                        iter_revision_index.modify( iter_revision_index.iterator_to( *rev_itr ), [&]( iterator_wrapper& i ){ --(i.iter); } );
                     }

                     // Decrement back to the first key that is less than the head key
                     while( !key_compare( key_from_value( *(rev_itr->iter) ), *head_key ) && rev_itr->iter != begin )
                     {
                        iter_revision_index.modify( iter_revision_index.iterator_to( *rev_itr ), [&]( iterator_wrapper& i ){ --(i.iter); } );
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
                        iter_revision_index.modify( iter_revision_index.iterator_to( *(rev_itr) ), [](iterator_wrapper& i ){ --(i.iter); } );
                     }
                  }
               }
            }

            const auto& rev_order_idx = iter_revision_index.template get< by_reverse_order_revision >();
            auto least_itr = rev_order_idx.begin();

            if( undo_deque.size() > 1 )
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
                  iter_revision_index.modify( iter_revision_index.iterator_to( *(least_itr--) ), [](iterator_wrapper& i ){ ++(i.iter); } );
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
            return iter_revision_index.begin()->iter.operator *();
         }

         const value_type* operator->()const
         {
            return iter_revision_index.begin()->iter.operator ->();
         }

         merge_iterator& operator =( const merge_iterator& other )
         {
            assert( &undo_deque == &(other.undo_deque) );
            iter_revision_index = other.iter_revision_index;
            base_revision = other.base_revision;

            return *this;
         }

         merge_iterator& operator =( merge_iterator&& other )
         {
            assert( &undo_deque == &(other.undo_deque) );
            iter_revision_index = std::move( other.iter_revision_index );
            base_revision = other.base_revision;

            return *this;
         }

      private:
         template< typename IterType >
         bool is_dirty( IterType itr )
         {
            bool dirty = false;

            for( size_t i = undo_deque.size() - 1; itr->revision < undo_deque[i]->revision() && !dirty; --i )
            {
               dirty = undo_deque[i]->is_modified( itr->iter->id );
            }

            return dirty;
         }

         void resolve_conflicts()
         {
            auto first_itr = iter_revision_index.begin();
            bool dirty = true;

            while( dirty && first_itr->valid() )
            {
               dirty = is_dirty( first_itr );

               if( dirty )
               {
                  iter_revision_index.modify( first_itr, [](iterator_wrapper& i ){ ++(i.iter); } );
               }

               first_itr = iter_revision_index.begin();
            }
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

         merge_index( const boost::container::deque< std::weak_ptr< state_delta_type > >& d )
         {
            for( auto& w : d )
            {
               auto maybe_state = w.lock();
               if( maybe_state ) _deque.push_back( maybe_state );
            }
         }

         template< typename CompatibleKey >
         iterator_type lower_bound( CompatibleKey&& key ) const
         {
            return iterator_type( _deque, [&]( by_index_type& idx )
            {
               return idx.lower_bound( key );
            });
         }

         template< typename CompatibleKey >
         iterator_type upper_bound( CompatibleKey&& key ) const
         {
            return iterator_type( _deque, [&]( by_index_type& idx )
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
            return iterator_type( _deque, [&]( by_index_type& idx )
            {
               return idx.begin();
            });
         }

         iterator_type end() const
         {
            return iterator_type( _deque, [&]( by_index_type& idx )
            {
               return idx.end();
            });
         }

         template< typename CompatibleKey >
         iterator_type find( CompatibleKey&& key ) const
         {
            return iterator_type( _deque, [&]( by_index_type& idx )
            {
               return idx.find( key );
            });
         }

         iterator_type iterator_to( const value_type& v ) const
         {
            return iterator_type( _deque, [&]( by_index_type& idx )
            {
               auto itr = idx.iterator_to( v );
               return itr != idx.end() ? itr : idx.upper_bound( v );
            });
         }

         boost::container::deque< std::shared_ptr< state_delta_type > > _deque;
   };

} // koinos::statedb
