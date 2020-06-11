
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/thunks.hpp>

#include <koinos/pack/classes.hpp>
#include <koinos/pack/rt/binary.hpp>

#include <boost/container/flat_map.hpp>

#include <functional>

namespace koinos { namespace chain { namespace detail {

using koinos::protocol::vl_blob;
using boost::container::flat_map;

class thunk_dispatcher_impl : public thunk_dispatcher
{
   public:
      // These parameters should be refeneces, but std::function doesn't play nicely with references.
      // See https://cboard.cprogramming.com/cplusplus-programming/160098-std-thread-help.html
      typedef std::function< void(apply_context*, vl_blob*, const vl_blob*) > generic_thunk_handler;

      thunk_dispatcher_impl();
      virtual ~thunk_dispatcher_impl();

      void register_thunks();
      template< typename ThunkRet, typename ThunkArgs, typename ThunkHandler >
      void register_thunk( thunk_id id, ThunkHandler handler );

      template< typename ThunkArgs, typename ThunkRet, typename ThunkHandler >
      static void generic_call_thunk( ThunkHandler handler, apply_context& ctx, vl_blob& vl_ret, const vl_blob& vl_args );

      void call_thunk( thunk_id id, apply_context& ctx, protocol::vl_blob& ret, const protocol::vl_blob& args ) override;

      static thunk_dispatcher_impl the_thunk_dispatcher;
      flat_map< thunk_id, generic_thunk_handler > _dispatch_map;
};

thunk_dispatcher_impl thunk_dispatcher_impl::the_thunk_dispatcher;
}

thunk_dispatcher::thunk_dispatcher() {}
thunk_dispatcher::~thunk_dispatcher() {}

thunk_dispatcher& thunk_dispatcher::instance()
{
   return detail::thunk_dispatcher_impl::the_thunk_dispatcher;
}

namespace detail {

thunk_dispatcher_impl::thunk_dispatcher_impl()
{
   register_thunks();
}

thunk_dispatcher_impl::~thunk_dispatcher_impl() {}

template< typename ThunkArgs, typename ThunkRet, typename ThunkHandler >
void thunk_dispatcher_impl::generic_call_thunk( ThunkHandler handler, apply_context& ctx, vl_blob& vl_ret, const vl_blob& vl_args )
{
   ThunkArgs args;
   ThunkRet ret;
   koinos::pack::from_vl_blob( vl_args, args );
   handler( ctx, ret, args );
   koinos::pack::to_vl_blob( vl_ret, ret );
}

template< typename ThunkRet, typename ThunkArgs, typename ThunkHandler >
void thunk_dispatcher_impl::register_thunk( thunk_id id, ThunkHandler handler )
{
   _dispatch_map.emplace( id, [handler]( apply_context* ctx, vl_blob* vl_ret, const vl_blob* vl_args )
   {
      thunk_dispatcher_impl::generic_call_thunk< ThunkArgs, ThunkRet, ThunkHandler >( handler, *ctx, *vl_ret, *vl_args );
   } );
}

void thunk_dispatcher_impl::call_thunk( thunk_id id, apply_context& ctx, protocol::vl_blob& ret, const protocol::vl_blob& args )
{
   auto it = _dispatch_map.find( id );
   KOINOS_ASSERT( it != _dispatch_map.end(), unknown_thunk, "Thunk ${id} not found", ("id", id) );
   it->second( &ctx, &ret, &args );
}

void thunk_dispatcher_impl::register_thunks()
{
   register_thunk< hello_thunk_ret, hello_thunk_args >( 1234, thunk::hello );
}

} } }
