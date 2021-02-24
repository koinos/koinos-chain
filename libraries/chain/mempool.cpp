#include <koinos/chain/mempool.hpp>

#include <functional>
#include <mutex>
#include <tuple>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <koinos/chain/thunks.hpp>

namespace koinos::chain {

namespace detail {

using namespace boost;

struct pending_transaction_object
{
   multihash             id;          //< Transaction ID
   block_height_type     last_update; //< Chain height at the time of submission
   protocol::transaction transaction;
};

struct by_id;
struct by_height;
struct by_height_id;

using pending_transaction_index = multi_index_container<
   pending_transaction_object,
   multi_index::indexed_by<
      multi_index::ordered_unique< multi_index::tag< by_id >,
         multi_index::member< pending_transaction_object, multihash, &pending_transaction_object::id >
      >,
      multi_index::ordered_non_unique< multi_index::tag< by_height >,
         multi_index::member< pending_transaction_object, block_height_type, &pending_transaction_object::last_update >
      >,
      multi_index::ordered_unique< multi_index::tag< by_height_id >,
         multi_index::composite_key< pending_transaction_object,
            multi_index::member< pending_transaction_object, block_height_type, &pending_transaction_object::last_update >,
            multi_index::member< pending_transaction_object, multihash, &pending_transaction_object::id >
         >
      >
   >
>;

struct account_resources_object
{
   account_type      account;
   uint128           resources;
   block_height_type last_update;
};

struct by_account;

using account_resources_index = multi_index_container<
   account_resources_object,
   multi_index::indexed_by<
      multi_index::ordered_unique< multi_index::tag< by_account >,
         multi_index::member< account_resources_object, account_type, &account_resources_object::account >
      >
   >
>;

class mempool_impl final
{
private:
   account_resources_index          _account_resources_idx;
   std::mutex                       _account_resources_mutex;

   pending_transaction_index        _pending_transaction_idx;
   std::mutex                       _pending_transaction_mutex;

   std::shared_ptr< apply_context > _ctx;

public:
   mempool_impl( std::shared_ptr< apply_context > _ctx );
   virtual ~mempool_impl();

   bool has_pending_transaction( const multihash& id );
   std::vector< protocol::transaction > get_pending_transactions( const multihash& start, std::size_t limit );
   void add_pending_transaction( const multihash& id, const protocol::transaction& t, block_height_type h );
   void remove_pending_transaction( const multihash& id );
   void prune( block_height_type h );
};

mempool_impl::mempool_impl( std::shared_ptr< apply_context > ctx ) : _ctx( ctx ) {}
mempool_impl::~mempool_impl() = default;

bool mempool_impl::has_pending_transaction( const multihash& id )
{
   std::lock_guard< std::mutex > guard( _pending_transaction_mutex );

   auto& id_idx = _pending_transaction_idx.get< by_id >();

   auto it = id_idx.find( id );

   return it != id_idx.end();
}

std::vector< protocol::transaction > mempool_impl::get_pending_transactions( const multihash& start, std::size_t limit )
{
   std::lock_guard< std::mutex > guard( _pending_transaction_mutex );

   auto& height_id_idx = _pending_transaction_idx.get< by_height_id >();

   std::vector< protocol::transaction > pending_transactions;

   auto it = height_id_idx.lower_bound( std::make_tuple( block_height_type{ 0 }, start ) );
   std::size_t count = 0;
   while ( it != height_id_idx.end() && count < limit )
   {
      pending_transactions.push_back( it->transaction );
      ++it;
      count++;
   }

   return pending_transactions;
}

void mempool_impl::add_pending_transaction( const multihash& id, const protocol::transaction& t, block_height_type h )
{
   auto account = thunk::get_transaction_payer( *_ctx, t );
   auto resource_limit = thunk::get_transaction_resource_limit( *_ctx, t );

   {
      std::lock_guard< std::mutex > guard( _account_resources_mutex );

      auto& account_idx = _account_resources_idx.get< by_account >();
      auto it = account_idx.find( account );

      if ( it == account_idx.end() )
      {
         auto max_resources = thunk::get_max_account_resources( *_ctx, account );

         KOINOS_ASSERT(
            resource_limit <= max_resources,
            transaction_exceeds_resources,
            "transaction would exceed maximum resources for account: ${a}", ("a", account)
         );

         _account_resources_idx.insert( account_resources_object {
            .account = account,
            .resources = max_resources - resource_limit,
            .last_update = h
         } );
      }
      else
      {
         KOINOS_ASSERT(
            resource_limit <= it->resources,
            transaction_exceeds_resources,
            "transaction would exceed resources for account: ${a}", ("a", account)
         );

         account_idx.modify( it, [&]( account_resources_object& aro )
         {
            aro.resources -= resource_limit;
            aro.last_update = h;
         } );
      }
   }

   {
      std::lock_guard< std::mutex > guard( _pending_transaction_mutex );

      auto rval = _pending_transaction_idx.insert( pending_transaction_object {
         .id = id,
         .last_update = h,
         .transaction = t
      } );

      KOINOS_ASSERT( rval.second, pending_transaction_insertion_failure, "failed to insert transaction with id: ${id}", ("id", id) );
   }
}

void mempool_impl::remove_pending_transaction( const multihash& id )
{
   std::lock_guard< std::mutex > guard( _pending_transaction_mutex );

   auto& id_idx = _pending_transaction_idx.get< by_id >();

   auto it = id_idx.find( id );
   if ( it != id_idx.end() )
   {
      id_idx.erase( it );
   }
}

void mempool_impl::prune( block_height_type h )
{
   std::lock_guard< std::mutex > guard( _pending_transaction_mutex );
}

} // detail

mempool::mempool( std::shared_ptr< apply_context > ctx ) : _my( std::make_unique< detail::mempool_impl >( ctx ) ) {}
mempool::~mempool() = default;

bool mempool::has_pending_transaction( const multihash& id )
{
   return _my->has_pending_transaction( id );
}

std::vector< protocol::transaction > mempool::get_pending_transactions( const multihash& start, std::size_t limit )
{
   return _my->get_pending_transactions( start, limit );
}

void mempool::add_pending_transaction( const multihash& id, const protocol::transaction& t, block_height_type h )
{
   _my->add_pending_transaction( id, t, h );
}

void mempool::remove_pending_transaction( const multihash& id )
{
   _my->remove_pending_transaction( id );
}

void mempool::prune( block_height_type h )
{
   _my->prune( h );
}

} // koinos::chain
