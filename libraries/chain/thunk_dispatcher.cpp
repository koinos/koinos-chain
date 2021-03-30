#include <koinos/chain/thunk_dispatcher.hpp>

namespace koinos::chain {

thunk_dispatcher::thunk_dispatcher()
{
   register_thunks( *this );
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
   it->second( ctx, ret_ptr, ret_len, arg_ptr, arg_len );
}

bool thunk_dispatcher::thunk_exists( thunk_id id ) const
{
   auto it = _dispatch_map.find( id );
   return it != _dispatch_map.end();
}

} // koinos::chain
