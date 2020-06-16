
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/thunks.hpp>

#include <koinos/pack/classes.hpp>
#include <koinos/pack/rt/binary.hpp>

namespace koinos::chain { namespace detail {

template< typename ThunkHandler, typename ThunkRet, typename... ThunkArgs >
void register_thunk( thunk_dispatcher& td, thunk_id id, ThunkHandler handler )
{
   _dispatch_map.emplace( id, [handler]( apply_context* ctx, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len )
   {
      pack::call_with_vl_blob< ThunkRet, ThunkArgs... >( handler, ret__ptr, ret_len, arg_ptr, arg_len, ctx );
   } );
   _pass_through_map.emplace( id, (void*)&handler );
}

void register_thunks( thunk_dispatcher& td, )
{
   register_thunk< hello_thunk_ret, hello_thunk_args >( 1234, thunk::hello );
}

} // detail

thunk_dispatcher::thunk_dispatcher()
{
   register_thunks( *this );
}

thunk_dispatcher::~thunk_dispatcher() {}

thunk_dispatcher& thunk_dispatcher::instance()
{
   static const thunk_dispatcher td;
   return td;
}

void thunk_dispatcher::call_thunk( thunk_id id, apply_context& ctx, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len )
{
   auto it = _dispatch_map.find( id );
   KOINOS_ASSERT( it != _dispatch_map.end(), unknown_thunk, "Thunk ${id} not found", ("id", id) );
   it->second( &ctx, &ret, &args );
}

} // koinos::chain
