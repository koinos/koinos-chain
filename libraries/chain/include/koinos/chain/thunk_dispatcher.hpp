#pragma once

#include <cstdint>

#include <koinos/chain/thunks.hpp>
#include <koinos/exception.hpp>

#include <koinos/pack/classes.hpp>
#include <koinos/pack/rt/binary.hpp>

#include <boost/container/flat_map.hpp>

#include <functional>
#include <type_traits>

namespace koinos { namespace protocol {
struct vl_blob;
} }

namespace koinos { namespace chain {

DECLARE_KOINOS_EXCEPTION( unknown_thunk );

class apply_context;

typedef uint32_t thunk_id;

namespace detail
{
   // An implementation of bind_front prior to official support in C++20 based on
   // https://codereview.stackexchange.com/questions/227695/c17-compatible-stdbind-front-alternative
   template< typename Function, typename BoundArg >
   struct bind_obj
   {
      Function& func;
      BoundArg arg;

      bind_obj( Function& f, BoundArg&& b ) :
         func( f ),
         arg( b )
      {}

      template< typename... CallArgs >
      inline auto operator()( CallArgs... call_args )
      {
         return std::invoke( func, arg, call_args... );
      }
   };

   template< typename Function, typename Arg >
   inline bind_obj< Function, Arg > bind_front( Function& func, Arg&& arg )
   {
      return bind_obj< Function, Arg >(
         func, std::forward< Arg >( arg )
      );
   }

   template< typename Function, typename Stream >
   inline Function parse_args( Function& f, Stream& )
   {
      return f;
   }

   template< typename Function, typename Stream, typename T, typename... Ts >
   inline auto parse_args( Function& f, Stream& s )
   {
      typename std::remove_const< typename std::remove_reference< T >::type >::type t;
      pack::from_binary( s, t );
      auto bound_func = bind_front( f, std::move(t) );
      return parse_args< decltype(bound_func), Stream, Ts... >( bound_func, s );
   }

   template< typename ThunkResult, typename... ThunkArgs >
   typename std::enable_if< std::is_same< ThunkResult, void >::value, int >::type
   call_thunk_impl( std::function< ThunkResult(apply_context&, ThunkArgs...) >& thunk, apply_context& ctx, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len )
   {
      pack::detail::input_stringstream instream( arg_ptr, arg_len );
      auto thunk_with_ctx = detail::bind_front( thunk, ctx );
      detail::parse_args< decltype(thunk_with_ctx), decltype(instream), ThunkArgs... >( thunk_with_ctx, instream )();
      return 0;
   }

   template< typename ThunkResult, typename... ThunkArgs >
   typename std::enable_if< !std::is_same< ThunkResult, void >::value, int >::type
   call_thunk_impl( std::function< ThunkResult(apply_context&, ThunkArgs...) >& thunk, apply_context& ctx, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len )
   {
      pack::detail::input_stringstream instream( arg_ptr, arg_len );
      auto thunk_with_ctx = detail::bind_front( thunk, ctx );
      pack::to_c_str< ThunkResult >( ret_ptr, ret_len, detail::parse_args< decltype(thunk_with_ctx), decltype(instream), ThunkArgs... >( thunk_with_ctx, instream )() );
      return 0;
   }
} // detail

/**
 * A registry for thunks.
 *
 * - A thunk is one-dimensional:  It's a call from WASM to native C++ code.
 * - A thunk is immutable:  A thunk always points to a particular native function.
 *
 * In particular, a semantically inequivalent upgrade to a thunk should never occur.
 * When there's a bug in a thunk implementation, the existing buggy implementation should
 * be kept under the same thunk ID.
 *
 * System governance should instead replace the system code that calls a particular
 * thunk ID with system code that calls a different thunk ID.
 */

class thunk_dispatcher
{
   public:
      void call_thunk( thunk_id id, apply_context& ctx, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len )const;

      template< typename ThunkHandler, typename... ThunkArgs >
      auto call_thunk( uint32_t id, apply_context& ctx, ThunkArgs... args ) const
      {
         auto it = _pass_through_map.find( id );
         KOINOS_ASSERT( it != _pass_through_map.end(), unknown_thunk, "Thunk ${id} not found", ("id", id) );
         return (*(ThunkHandler*)(it->second))( ctx, args... );
      }

      template< typename ThunkResult, typename... ThunkArgs >
      void register_thunk( thunk_id id, ThunkResult (*thunk_ptr)(apply_context&, ThunkArgs...) )
      {
         _dispatch_map.emplace( id, [thunk_ptr]( apply_context& ctx, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len )
         {
            std::function< ThunkResult(apply_context&, ThunkArgs...) > thunk = thunk_ptr;
            detail::call_thunk_impl( thunk, ctx, ret_ptr, ret_len, arg_ptr, arg_len );
         });
         _pass_through_map.emplace( id, (void*)&thunk_ptr );
      }

      static const thunk_dispatcher& instance();

   private:
      thunk_dispatcher();

      // These parameters should be refeneces, but std::function doesn't play nicely with references.
      // See https://cboard.cprogramming.com/cplusplus-programming/160098-std-thread-help.html
      typedef std::function< void(apply_context&, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len) > generic_thunk_handler;

      boost::container::flat_map< thunk_id, generic_thunk_handler >  _dispatch_map;
      boost::container::flat_map< thunk_id, void* >                  _pass_through_map;
};

} }
