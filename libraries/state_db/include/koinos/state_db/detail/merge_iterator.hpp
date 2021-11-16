#pragma once

#include <koinos/state_db/detail/state_delta.hpp>

#include <boost/iterator/zip_iterator.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <deque>

namespace koinos::state_db::detail {

   using namespace boost::multi_index;

   class merge_iterator :
      public boost::bidirectional_iterator_helper<
         typename merge_iterator,
         typename state_delta::value_type,
         std::size_t,
         const typename state_delta::value_type*,
         const typename state_delta::value_type& >
   {
      public:
         using key_type   = state_delta::key_type;
         using value_type = state_delta::value_type;
      private:
         using iterator_type   = backends::iterator;
         using backend_type    = std::shared_ptr< backends::abstract_backend >;
         using state_delta_ptr = std::shared_ptr< state_delta >;

         struct iterator_wrapper
         {
            iterator_wrapper( iterator_type&& i, uint64_t r, const backend_type b ) :
               _itr( std::move( i ) ),
               _revision( r ),
               _backend( b )
            {}

            iterator_wrapper( iterator_wrapper&& i ) :
               _itr( std::move( i._itr ) ),
               _revision( i._revision ),
               _backend( i._backend )
            {}

            iterator_wrapper( const iterator_wrapper& i ) :
               _itr( i._itr ),
               _revision( i._revision ),
               _backend( i._backend )
            {}

            iterator_type      _itr;
            uint64_t           _revision;
            const backend_type _backend;

            const iterator_wrapper& self() const { return *this; }
            bool valid() const { return _itr != _backend->end(); }
         };

         // Uses _revision as a tiebreaker only for when both iterators are invalid
         // to enforce a total ordering on this comparator. The composite key on
         // _revision is still needed for the case when iterators are valid and equal.
         // (i.e. lhs < rhs == false && rhs < lhs == false )
         struct iterator_compare_less
         {
            bool operator()( const iterator_wrapper& lhs, const iterator_wrapper& rhs )const
            {
               bool lh_valid = lhs.valid();
               bool rh_valid = rhs.valid();

               if ( !lh_valid && !rh_valid ) return lhs._revision > rhs._revision;
               if ( !lh_valid ) return false;
               if ( !rh_valid ) return true;

               return lhs.itr.key() < rhs._itr.key();
            }
         };

         struct iterator_compare_greater
         {
            bool operator()( const iterator_wrapper& lhs, const iterator_wrapper& rhs )const
            {
               bool lh_valid = lhs.valid();
               bool rh_valid = rhs.valid();

               if ( !lh_valid && !rh_valid ) return lhs._revision > rhs._revision;
               if ( !lh_valid ) return false;
               if ( !rh_valid ) return true;

               return rhs._itr.key() < lhs._itr.key();
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
                     member< iterator_wrapper, uint64_t, &iterator_wrapper::_revision >
                  >,
                  composite_key_compare< iterator_compare_less, std::greater< uint64_t > >
               >,
               ordered_unique< tag< by_reverse_order_revision >,
                  composite_key< iterator_wrapper,
                     const_mem_fun< iterator_wrapper, const iterator_wrapper&, &iterator_wrapper::self >,
                     member< iterator_wrapper, uint64_t, &iterator_wrapper::_revision >
                  >,
                  composite_key_compare< iterator_compare_greater, std::greater< uint64_t > >
               >,
               ordered_unique< tag< by_revision >, member< iterator_wrapper, uint64_t, &iterator_wrapper::_revision > >
            >
         > iter_revision_index_type;

         iter_revision_index_type      _itr_revision_index;
         std::deque< state_delta_ptr > _delta_deque;

      public:
         template< typename Initializer >
         merge_iterator( state_delta_ptr head, Initializer&& init )
         {
            KOINOS_ASSERT( head, internal_error, "Cannot create a merge iterator on an null delta." );
            auto current_delta = head;

            do
            {
               _delta_deque.push_front( current_delta );

               _itr_revision_index.emplace(
                  iterator_wrapper(
                     std::move( init( current_delta->_backend ) ),
                     current_delta->revision(),
                     current_delta->indices() )
                  )
               );

               current_delta = current_delta->parent();
            } while( current_delta );

            resolve_conflicts();
         }

         merge_iterator( const std::deque< state_delta_ptr >& deque ) :
            _delta_deque( deque )
         {}

         merge_iterator( const merge_iterator& other ) :
            _itr_revision_index( other._itr_revision_index ),
            _delta_deque( other._delta_deque )
         {}

         merge_iterator( merge_iterator&& other ) :
            _itr_revision_index( std::move( other._itr_revision_index ) ),
            _delta_deque( other._delta_deque )
         {}

         merge_iterator() {}

         bool operator ==( const merge_iterator& other )const
         {
            // If both iterators are empty, they are true.
            // But we use empty merge iterators as an optimization for an end itertor.
            // So if one is empty, and the other is all end iterators, they are also equal.
            if ( _itr_revision_index.size() == 0 && other._itr_revision_index.size() == 0 ) return true;
            else if ( _itr_revision_index.size() == 0 ) return other.is_end();
            else if ( other._itr_revision_index.size() == 0 ) return is_end();

            auto my_begin = _itr_revision_index.begin();
            auto other_begin = other._itr_revision_index.begin();

            if ( !my_begin->valid() && !other_begin->valid() ) return true;
            if ( !my_begin->valid() || !other_begin->valid() ) return false;
            if ( my_begin->_revision != other_begin->_revision ) return false;

            return my_begin->_itr == other_begin->_itr;
         }

         merge_iterator& operator ++()
         {
            auto first_itr = _itr_revision_index.begin();

            if ( first_itr->valid() )
            {
               _itr_revision_index.modify( first_itr, []( iterator_wrapper& i ){ ++(i.iter); } );
               resolve_conflicts();
            }

            return *this;
         }

         merge_iterator operator ++(int)const
         {
            return ++( merge_iterator( *this ) );
         }

         merge_iterator& operator --()
         {
            const auto& order_idx = _itr_revision_index.template get< by_order_revision >();

            auto head_itr = order_idx.begin();
            // composite keys do not have default initializers, so we need to store them as a pointer
            std::optional< const key_type& > head_key;

            if( head_itr->valid() )
            {
               head_key = head_itd->_itr.key();
            }

            /* We are grabbing the current head value.
             * Then iterate over all other iterators and rewind them until they have a value less
             * than the current value. One of those values is what we want to decrement to.
             */
            const auto& rev_idx = _itr_revision_index.template get< by_revision >();
            for( auto rev_itr = rev_idx.begin(); rev_itr != rev_idx.end(); ++rev_itr )
            {
               // Only decrement iterators that have modified objects
               if( rev_itr->_backend->size() )
               {
                  auto begin = rev_itr->_backend->begin();

                  if( !head_key )
                  {
                     // If there was no valid key, then bring back each iterator once, it is gauranteed to be less than the
                     // current value (end()).
                     _itr_revision_index.modify( _itr_revision_index.iterator_to( *rev_itr ), [&]( iterator_wrapper& i ){ --(i.iter); } );
                  }
                  else
                  {
                     // Do an initial decrement if the iterator currently points to end()
                     if( !rev_itr->valid() )
                     {
                        _itr_revision_index.modify( _itr_revision_index.iterator_to( *rev_itr ), [&]( iterator_wrapper& i ){ --(i.iter); } );
                     }

                     // Decrement back to the first key that is less than the head key
                     while( rev_itr->_itr.key() >= *head_key && rev_itr->_itr != begin )
                     {
                        _itr_revision_index.modify( _itr_revision_index.iterator_to( *rev_itr ), [&]( iterator_wrapper& i ){ --(i.iter); } );
                     }
                  }

                  // The key at this point is guaranteed to be less than the head key (or at begin() and greator), but it
                  // might have been modified in a later index. We need to continue decrementing until we have a valid key.
                  bool dirty = true;

                  while( dirty && rev_itr->valid() && rev_itr->_itr != begin )
                  {
                     dirty = is_dirty( rev_itr );

                     if( dirty )
                     {
                        _itr_revision_index.modify( _itr_revision_index.iterator_to( *(rev_itr) ), [](iterator_wrapper& i ){ --(i.iter); } );
                     }
                  }
               }
            }

            const auto& rev_order_idx = _itr_revision_index.template get< by_reverse_order_revision >();
            auto least_itr = rev_order_idx.begin();

            if( _delta_deque.size() > 1 )
            {
               // This next bit works in two modes.
               // Some indices may not have had a value less than the previous head, so they will show up first,
               // we need to increment through those values until we get the the new valid least value.
               if( head_key )
               {
                  while( least_itr != rev_order_idx.end() && least_itr->valid()
                     && ( is_dirty( least_itr ) || lead_itr->_itr.key() >= *head_key ) )
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
                  _itr_revision_index.modify( _itr_revision_index.iterator_to( *(least_itr--) ), [](iterator_wrapper& i ){ ++(i.iter); } );
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
            return _itr_revision_index.begin()->_itr.operator *();
         }

         const value_type* operator->()const
         {
            return _itr_revision_index.begin()->_itr.operator ->();
         }

         merge_iterator& operator =( const merge_iterator& other )
         {
            KOINOS_ASSERT( _delta_deque.size(), internal_error, "Merge iterator is unexpectedly empty" );
            KOINOS_ASSERT( _delta_deque.size() == other._delta_deque.size(), internal_error, "Cannot assign iterators with different delta deques." );
            KOINOS_ASSERT( _delta_deque.begin()->id() == _delta_deque.begin()->id(), internal_error, "Cannot assign merge iterators with different roots" );
            KOINOS_ASSERT( _delta_deque.rbegin()->id() == _delta_deque.rbegin()->id(), internal_error, "Cannot assign merge iterators with different heads" );

            _itr_revision_index = other._itr_revision_index;

            return *this;
         }

         merge_iterator& operator =( merge_iterator&& other )
         {
            KOINOS_ASSERT( _delta_deque.size(), internal_error, "Merge iterator is unexpectedly empty" );
            KOINOS_ASSERT( _delta_deque.size() == other._delta_deque.size(), internal_error, "Cannot assign iterators with different delta deques." );
            KOINOS_ASSERT( _delta_deque.begin()->id() == _delta_deque.begin()->id(), internal_error, "Cannot assign merge iterators with different roots" );
            KOINOS_ASSERT( _delta_deque.rbegin()->id() == _delta_deque.rbegin()->id(), internal_error, "Cannot assign merge iterators with different heads" );

            _itr_revision_index = std::move( other._itr_revision_index );

            return *this;
         }

      private:
         bool is_dirty( iterator_wrapper itr )
         {
            bool dirty = false;

            for( int i = _delta_deque.size() - 1; itr->_revision < _delta_deque[i]->revision() && !dirty; --i )
            {
               dirty = _delta_deque[i]->is_modified( itr->iter->id );
            }

            return dirty;
         }

         void resolve_conflicts()
         {
            auto first_itr = _itr_revision_index.begin();
            bool dirty = true;

            while( dirty && first_itr->valid() )
            {
               dirty = is_dirty( first_itr );

               if( dirty )
               {
                  _itr_revision_index.modify( first_itr, [](iterator_wrapper& i ){ ++(i.iter); } );
               }

               first_itr = _itr_revision_index.begin();
            }
         }

         bool is_end() const
         {
            return std::all_of( _itr_revision_index.begin(), _itr_revision_index.end(),
               []( auto& i ){ return !i.valid(); } );
         }
   };

   template< typename MultiIndexType, typename IndexedByType >
   class merge_index
   {
      public:
         using backend_type     = backends::abstract_backend;
         using state_delta_type = state_delta;
         using key_type         = state_delta::key_type;

         std::shared_ptr< state_delta_type > _head;

         merge_index( std::shared_ptr< state_delta_type > head ) :
            _head( head )
         {}

         merge_iterator begin() const
         {
            return merge_iterator( _head, [&]( backend_type& backend )
            {
               return backend.begin();
            });
         }

         merge_iterator end() const
         {
            return merge_iterator( _head, [&]( backend_type& backend )
            {
               return backend.end();
            });
         }

         merge_iterator find( const key_type& key ) const
         {
            return merge_iterator( _head, [&]( backend_type& backend )
            {
               return backend.find( key );
            });
         }

         merge_iterator lower_bound( const key_type& key ) const
         {
            return merge_iterator( _head, [&]( backend_type& backend )
            {
               return backend.lower_bound( key );
            });
         }
   };

} // koinos::state_db::detail
