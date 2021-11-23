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

struct iterator_wrapper
{
   iterator_wrapper( backends::iterator&& i, uint64_t r, std::shared_ptr< backends::abstract_backend > b );
   iterator_wrapper( iterator_wrapper&& i );
   iterator_wrapper( const iterator_wrapper& i );

   const iterator_wrapper& self() const;
   bool valid() const;

   backends::iterator                            itr;
   std::shared_ptr< backends::abstract_backend > backend;
   uint64_t                                      revision;
};

// Uses revision as a tiebreaker only for when both iterators are invalid
// to enforce a total ordering on this comparator. The composite key on
// revision is still needed for the case when iterators are valid and equal.
// (i.e. lhs < rhs == false && rhs < lhs == false )
struct iterator_compare_less
{
   bool operator()( const iterator_wrapper& lhs, const iterator_wrapper& rhs ) const;
};

struct iterator_compare_greater
{
   bool operator()( const iterator_wrapper& lhs, const iterator_wrapper& rhs ) const;
};

class merge_iterator :
   public boost::bidirectional_iterator_helper<
      merge_iterator,
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
      using state_delta_ptr = std::shared_ptr< state_delta >;

      struct by_order_revision;
      struct by_reverse_order_revision;
      struct by_revision;

      using iter_revision_index_type = multi_index_container<
         iterator_wrapper,
         indexed_by<
            ordered_unique< tag< by_order_revision >,
               composite_key< iterator_wrapper,
                  const_mem_fun< iterator_wrapper, const iterator_wrapper&, &iterator_wrapper::self >,
                  member< iterator_wrapper, uint64_t, &iterator_wrapper::revision >
               >,
               composite_key_compare< iterator_compare_less, std::greater< uint64_t > >
            >,
            ordered_unique< tag< by_reverse_order_revision >,
               composite_key< iterator_wrapper,
                  const_mem_fun< iterator_wrapper, const iterator_wrapper&, &iterator_wrapper::self >,
                  member< iterator_wrapper, uint64_t, &iterator_wrapper::revision >
               >,
               composite_key_compare< iterator_compare_greater, std::greater< uint64_t > >
            >,
            ordered_unique< tag< by_revision >, member< iterator_wrapper, uint64_t, &iterator_wrapper::revision > >
         >
      >;

      iter_revision_index_type      _itr_revision_index;
      std::deque< state_delta_ptr > _delta_deque;

   public:
      template< typename Initializer >
      merge_iterator( state_delta_ptr head, Initializer&& init )
      {
         KOINOS_ASSERT( head, internal_error, "cannot create a merge iterator on a null delta" );
         auto current_delta = head;

         do
         {
            _delta_deque.push_front( current_delta );

            _itr_revision_index.emplace(
               iterator_wrapper(
                  std::move( init( current_delta->backend() ) ),
                  current_delta->revision(),
                  current_delta->backend()
               )
            );

            current_delta = current_delta->parent();
         } while( current_delta );

         resolve_conflicts();
      }

      merge_iterator( const merge_iterator& other );

      bool operator ==( const merge_iterator& other ) const;

      merge_iterator& operator++();
      merge_iterator& operator--();

      const value_type& operator*() const;

      const key_type& key() const;

   private:
      template< typename ItrType >
      bool is_dirty( ItrType itr )
      {
         bool dirty = false;

         for( int i = _delta_deque.size() - 1; itr->revision < _delta_deque[i]->revision() && !dirty; --i )
         {
            dirty = _delta_deque[i]->is_modified( itr->itr.key() );
         }

         return dirty;
      }

      void resolve_conflicts();
      bool is_end() const;
};

class merge_state
{
   public:
      using key_type         = state_delta::key_type;
      using value_type       = state_delta::value_type;

      merge_state( std::shared_ptr< state_delta > head );

      merge_iterator begin() const;
      merge_iterator end() const;

      const value_type* find( const key_type& key ) const;
      merge_iterator lower_bound( const key_type& key ) const;

   private:
      std::shared_ptr< state_delta > _head;
};

} // koinos::state_db::detail
