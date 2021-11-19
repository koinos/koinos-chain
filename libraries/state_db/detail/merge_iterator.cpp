#include <koinos/state_db/detail/merge_iterator.hpp>

namespace koinos::state_db::detail {

iterator_wrapper::iterator_wrapper( backends::iterator&& i, uint64_t r, std::shared_ptr< backends::abstract_backend > b ) :
   itr( std::move( i ) ),
   revision( r ),
   backend( b )
{}

iterator_wrapper::iterator_wrapper( iterator_wrapper&& i ) :
   itr( std::move( i.itr ) ),
   revision( i.revision ),
   backend( i.backend )
{}

iterator_wrapper::iterator_wrapper( const iterator_wrapper& i ) :
   itr( i.itr ),
   revision( i.revision ),
   backend( i.backend )
{}

const iterator_wrapper& iterator_wrapper::self()const
{
   return *this;
}

bool iterator_wrapper::valid()const
{
   return itr != backend->end();
}

bool iterator_compare_less::operator()( const iterator_wrapper& lhs, const iterator_wrapper& rhs )const
{
   bool lh_valid = lhs.valid();
   bool rh_valid = rhs.valid();

   if ( !lh_valid && !rh_valid ) return lhs.revision > rhs.revision;
   if ( !lh_valid ) return false;
   if ( !rh_valid ) return true;

   return lhs.itr.key() < rhs.itr.key();
}

bool iterator_compare_greater::operator()( const iterator_wrapper& lhs, const iterator_wrapper& rhs )const
{
   bool lh_valid = lhs.valid();
   bool rh_valid = rhs.valid();

   if ( !lh_valid && !rh_valid ) return lhs.revision > rhs.revision;
   if ( !lh_valid ) return false;
   if ( !rh_valid ) return true;

   return rhs.itr.key() < lhs.itr.key();
}

merge_iterator::merge_iterator( const merge_iterator& other ) :
   _itr_revision_index( other._itr_revision_index ),
   _delta_deque( other._delta_deque )
{}

bool merge_iterator::operator==( const merge_iterator& other )const
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
   if ( my_begin->revision != other_begin->revision ) return false;

   return my_begin->itr == other_begin->itr;
}

merge_iterator& merge_iterator::operator++()
{
   auto first_itr = _itr_revision_index.begin();
   KOINOS_ASSERT( first_itr->valid(), koinos::exception, "" );

   _itr_revision_index.modify( first_itr, []( iterator_wrapper& i ){ ++(i.itr); } );
   resolve_conflicts();

   return *this;
}

merge_iterator& merge_iterator::operator--()
{
   const auto& order_idx = _itr_revision_index.template get< by_order_revision >();

   auto head_itr = order_idx.begin();
   std::optional< key_type > head_key;

   if( head_itr->valid() )
   {
      head_key = head_itr->itr.key();
   }

   /* We are grabbing the current head value.
    * Then iterate over all other iterators and rewind them until they have a value less
    * than the current value. One of those values is what we want to decrement to.
    */
   const auto& rev_idx = _itr_revision_index.template get< by_revision >();
   for( auto rev_itr = rev_idx.begin(); rev_itr != rev_idx.end(); ++rev_itr )
   {
      // Only decrement iterators that have modified objects
      if( rev_itr->backend->size() )
      {
         auto begin = rev_itr->backend->begin();

         if( !head_key )
         {
            // If there was no valid key, then bring back each iterator once, it is gauranteed to be less than the
            // current value (end()).
            _itr_revision_index.modify( _itr_revision_index.iterator_to( *rev_itr ), [&]( iterator_wrapper& i ){ --(i.itr); } );
         }
         else
         {
            // Do an initial decrement if the iterator currently points to end()
            if( !rev_itr->valid() )
            {
               _itr_revision_index.modify( _itr_revision_index.iterator_to( *rev_itr ), [&]( iterator_wrapper& i ){ --(i.itr); } );
            }

            // Decrement back to the first key that is less than the head key
            while( rev_itr->itr.key() >= *head_key && rev_itr->itr != begin )
            {
               _itr_revision_index.modify( _itr_revision_index.iterator_to( *rev_itr ), [&]( iterator_wrapper& i ){ --(i.itr); } );
            }
         }

         // The key at this point is guaranteed to be less than the head key (or at begin() and greator), but it
         // might have been modified in a later index. We need to continue decrementing until we have a valid key.
         bool dirty = true;

         while( dirty && rev_itr->valid() && rev_itr->itr != begin )
         {
            dirty = is_dirty( rev_itr );

            if( dirty )
            {
               _itr_revision_index.modify( _itr_revision_index.iterator_to( *(rev_itr) ), [](iterator_wrapper& i ){ --(i.itr); } );
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
            && ( is_dirty( least_itr ) || least_itr->itr.key() >= *head_key ) )
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
         _itr_revision_index.modify( _itr_revision_index.iterator_to( *(least_itr--) ), [](iterator_wrapper& i ){ ++(i.itr); } );
         ++least_itr;
      }

      resolve_conflicts();
   }

   return *this;
}

const merge_iterator::value_type& merge_iterator::operator*()const
{
   return _itr_revision_index.begin()->itr.operator *();
}

const merge_iterator::key_type& merge_iterator::key()const
{
   auto first_itr = _itr_revision_index.begin();
   KOINOS_ASSERT( first_itr->valid(), koinos::exception, "" );

   return first_itr->itr.key();
}

void merge_iterator::resolve_conflicts()
{
   auto first_itr = _itr_revision_index.begin();
   bool dirty = true;

   while( dirty && first_itr->valid() )
   {
      dirty = is_dirty( first_itr );

      if( dirty )
      {
         _itr_revision_index.modify( first_itr, [](iterator_wrapper& i ){ ++(i.itr); } );
      }

      first_itr = _itr_revision_index.begin();
   }
}

bool merge_iterator::is_end() const
{
   return std::all_of( _itr_revision_index.begin(), _itr_revision_index.end(),
      []( auto& i ){ return !i.valid(); } );
}

merge_state::merge_state( std::shared_ptr< state_delta > head ) :
   _head( head )
{}

merge_iterator merge_state::begin()const
{
   return merge_iterator( _head, [&]( std::shared_ptr< backends::abstract_backend > backend )
   {
      return backend->begin();
   });
}

merge_iterator merge_state::end()const
{
   return merge_iterator( _head, [&]( std::shared_ptr< backends::abstract_backend > backend )
   {
      return backend->end();
   });
}

const merge_state::value_type* merge_state::find( const key_type& key )const
{
   return _head->find( key );
}

merge_iterator merge_state::lower_bound( const key_type& key )const
{
   return merge_iterator( _head, [&]( std::shared_ptr< backends::abstract_backend > backend )
   {
      return backend->lower_bound( key );
   });
}

} // koinos::state_db::detail
