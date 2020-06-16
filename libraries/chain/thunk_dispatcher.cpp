
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/thunks.hpp>

namespace koinos::chain { namespace detail {

void register_thunks( thunk_dispatcher& td )
{
   td.register_thunk< hello_thunk_ret >( 1234, thunk::hello );
}

} // detail

thunk_dispatcher::thunk_dispatcher()
{
   detail::register_thunks( *this );
}

const thunk_dispatcher& thunk_dispatcher::instance()
{
   static const thunk_dispatcher td;
   return td;
}

void thunk_dispatcher::call_thunk( thunk_id id, apply_context& ctx, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len )const
{
   auto it = _dispatch_map.find( id );
   KOINOS_ASSERT( it != _dispatch_map.end(), unknown_thunk, "Thunk ${id} not found", ("id", id) );
   it->second( &ctx, ret_ptr, ret_len, arg_ptr, arg_len );
}

} // koinos::chain
