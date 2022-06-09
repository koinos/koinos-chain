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

void thunk_dispatcher::call_thunk( uint32_t id, execution_context& ctx, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len, uint32_t* bytes_written )const
{
   auto it = _dispatch_map.find( id );
   KOINOS_ASSERT( it != _dispatch_map.end(), unknown_thunk_exception, "thunk ${id} not found", ("id", id) );
   it->second( ctx, ret_ptr, ret_len, arg_ptr, arg_len, bytes_written );
}

bool thunk_dispatcher::thunk_exists( uint32_t id ) const
{
   return _dispatch_map.count( id );
}

bool thunk_dispatcher::thunk_is_genesis( uint32_t id ) const
{
   return _genesis_thunks.count( id );
}

} // koinos::chain
