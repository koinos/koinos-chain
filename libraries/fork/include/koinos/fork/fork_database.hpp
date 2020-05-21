#pragma once
#include <koinos/exception.hpp>
#include <koinos/fork/block_state.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>

namespace koinos::fork {

DECLARE_KOINOS_EXCEPTION( unlinkable_block_exception );
DECLARE_KOINOS_EXCEPTION( invalid_state_exception );
DECLARE_KOINOS_EXCEPTION( block_not_found_exception );
DECLARE_KOINOS_EXCEPTION( duplicate_block_exception );

template< typename BlockType >
class fork_database final
{
   public:
      fork_database()  = default;
      ~fork_database() = default;

      using block_state_type = block_state< BlockType >;
      using block_state_ptr  = std::shared_ptr< block_state_type >;
      using block_list_type  = std::vector< block_state_ptr >;
      using branch_pair_type = std::pair< block_list_type, block_list_type >;
      using block_id_type    = typename block_state< BlockType >::block_id_type;
      using block_num_type   = typename block_state< BlockType >::block_num_type;

      void                   reset( const block_state_ptr& b = block_state_ptr() );
      void                   add( const block_state_ptr& b, bool ignore_duplicate = true );
      void                   remove( const block_id_type& b );
      block_state_ptr        fetch_block( const block_id_type& id ) const;
      block_list_type        fetch_block_by_number( block_num_type n ) const;
      branch_pair_type       fetch_branch_from( const block_id_type& first, const block_id_type& second ) const;
      block_state_ptr        search_on_branch( const block_id_type& block_id, block_num_type block_num ) const;
      void                   advance_root( const block_id_type& b );
      const block_state_ptr& head() const;
      const block_state_ptr& root() const;

      size_t                 size() const;

   private:
      struct by_block_id;
      struct by_block_num;
      struct by_previous;

      using fork_multi_index_type = boost::multi_index_container<
         block_state_ptr,
         boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<
               boost::multi_index::tag< by_block_id >,
                  boost::multi_index::const_mem_fun< block_state_type, block_id_type, &block_state_type::id >
            >,
            boost::multi_index::ordered_non_unique<
               boost::multi_index::tag< by_previous >,
                  boost::multi_index::const_mem_fun< block_state_type, block_id_type, &block_state_type::previous_id >
            >,
            boost::multi_index::ordered_non_unique<
               boost::multi_index::tag< by_block_num >,
                  boost::multi_index::const_mem_fun< block_state_type, block_num_type, &block_state_type::block_num >
            >
         >
      >;

      fork_multi_index_type _index;
      block_state_ptr       _head;
      block_state_ptr       _root;
};

template< typename BlockType >
void fork_database< BlockType >::reset( const block_state_ptr& b )
{
   _index.clear();
   _root = b;
   _head = _root;
}

template< typename BlockType >
const typename fork_database< BlockType >::block_state_ptr& fork_database< BlockType >::head() const
{
   return _head;
}

template< typename BlockType >
const typename fork_database< BlockType >::block_state_ptr& fork_database< BlockType >::root() const
{
   return _root;
}

template< typename BlockType >
void fork_database< BlockType >::add( const block_state_ptr& b, bool ignore_duplicate )
{
   KOINOS_ASSERT( _root, invalid_state_exception, "root not yet set" );
   KOINOS_ASSERT( b != nullptr, invalid_state_exception, "attempt to add null block state" );

   block_state_ptr prev_block;

   if ( b->previous_id() == _root->id() )
      prev_block = _root;
   else
      prev_block = fetch_block( b->previous_id() );

   KOINOS_ASSERT( prev_block, unlinkable_block_exception, "block id: ${id}", ("id", b->id()) );

   auto inserted = _index.insert( b );
   if ( !inserted.second )
   {
      if ( !ignore_duplicate )
      {
         KOINOS_THROW( duplicate_block_exception, "block id: ${id}", ("id", b->id()) );
      }
   }
   else if ( b->block_num() > _head->block_num() )
   {
      _head = b;
   }
}

template< typename BlockType >
typename fork_database< BlockType >::block_state_ptr fork_database< BlockType >::fetch_block( const block_id_type& id ) const
{
   auto& index = _index.template get< by_block_id >();
   auto itr = index.find( id );
   if ( itr != index.end() )
      return *itr;

   return block_state_ptr();
}

template< typename BlockType >
typename fork_database< BlockType >::block_list_type fork_database< BlockType >::fetch_block_by_number( block_num_type num ) const
{
   block_list_type result;
   auto const& block_num_idx = _index.template get< by_block_num >();
   auto itr = block_num_idx.lower_bound( num );

   while ( itr != block_num_idx.end() && itr->get()->block_num() == num )
   {
      if ( (*itr)->block_num() == num )
         result.push_back( *itr );
      else
         break;
      ++itr;
   }

   return result;
}

template< typename BlockType >
typename fork_database< BlockType >::branch_pair_type fork_database< BlockType >::fetch_branch_from( const block_id_type& first, const block_id_type& second ) const
{
   branch_pair_type result;
   auto first_branch = ( first == _root->id() ) ? _root : fetch_block( first );
   auto second_branch = ( second == _root->id() ) ? _root : fetch_block( second );

   KOINOS_ASSERT( first_branch, block_not_found_exception, "block ${id} does not exist", ("id", first) );
   KOINOS_ASSERT( second_branch, block_not_found_exception, "block ${id} does not exist", ("id", second) );

   while ( first_branch->block_num() > second_branch->block_num() )
   {
      result.first.push_back( first_branch );
      const auto& prev = first_branch->previous_id();
      first_branch = ( prev == _root->id() ) ? _root : fetch_block( prev );
      KOINOS_ASSERT( first_branch, block_not_found_exception, "block ${id} does not exist", ("id", prev) );
   }

   while ( second_branch->block_num() > first_branch->block_num() )
   {
      result.second.push_back( second_branch );
      const auto& prev = second_branch->previous_id();
      second_branch = ( prev == _root->id() ) ? _root : fetch_block( prev );
      KOINOS_ASSERT( second_branch, block_not_found_exception, "block ${id} does not exist", ("id", prev) );
   }

   if ( first_branch->id() == second_branch->id() ) return result;

   while ( first_branch->previous_id() != second_branch->previous_id() )
   {
      result.first.push_back( first_branch );
      result.second.push_back( second_branch );
      const auto &first_prev = first_branch->previous_id();
      first_branch = fetch_block( first_prev );
      const auto &second_prev = second_branch->previous_id();
      second_branch = fetch_block( second_prev );
      KOINOS_ASSERT( first_branch, block_not_found_exception, "block ${id} does not exist", ("id", first_prev) );
      KOINOS_ASSERT( second_branch, block_not_found_exception, "block ${id} does not exist", ("id", second_prev) );
   }

   if( first_branch && second_branch )
   {
      result.first.push_back( first_branch );
      result.second.push_back( second_branch );
   }

   return result;
}

template< typename BlockType >
void fork_database< BlockType >::remove( const block_id_type& id )
{
   std::vector< block_id_type > remove_queue{ id };
   const auto& previdx = _index.template get< by_previous >();
   const auto head_id = _head->id();

   for ( uint32_t i = 0; i < remove_queue.size(); ++i )
   {
      KOINOS_ASSERT( remove_queue[ i ] != head_id, invalid_state_exception, "removing the block and its descendants would remove the current head block" );

      auto previtr = previdx.lower_bound( remove_queue[ i ] );
      while ( previtr != previdx.end() && (*previtr)->previous_id() == remove_queue[ i ] )
      {
         remove_queue.push_back( (*previtr)->id() );
         ++previtr;
      }
   }

   for ( const auto& block_id : remove_queue )
   {
      auto itr = _index.find( block_id );
      if ( itr != _index.end() )
         _index.erase( itr );
   }
}

template< typename BlockType >
typename fork_database< BlockType >::block_state_ptr fork_database< BlockType >::search_on_branch( const block_id_type& block_id, block_num_type block_num ) const
{
   for ( auto b = fetch_block( block_id ); b != nullptr; b = fetch_block( b->previous_id() ) )
   {
      if ( b->block_num() == block_num )
         return b;
   }

   return {};
}

template< typename BlockType >
void fork_database< BlockType >::advance_root( const block_id_type& id )
{
   KOINOS_ASSERT( _root, invalid_state_exception, "root not yet set" );

   auto new_root = fetch_block( id );
   KOINOS_ASSERT( new_root, invalid_state_exception, "cannot advance root to a block that does not exist in the fork database" );

   std::vector< block_id_type > block_removal_queue;
   for ( auto b = new_root; b; )
   {
      block_removal_queue.push_back( b->previous_id() );
      b = fetch_block( block_removal_queue.back() );
      KOINOS_ASSERT( b || block_removal_queue.back() == _root->id(), invalid_state_exception, "orphaned branch was present in forked database" );
   }

   // The new root block should be erased from the fork database index individually rather than with the remove method,
   // because we do not want the blocks branching off of it to be removed from the fork database.
   _index.erase( _index.find( id ) );

   // The other blocks to be removed are removed using the remove method so that orphaned branches do not remain in the fork database.
   for ( const auto& block_id : block_removal_queue )
      remove( block_id );

   _root = new_root;
}

template< typename BlockType >
size_t fork_database< BlockType >::size() const
{
   return _index.size();
}

} // koinos::fork
