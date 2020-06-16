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
   template< typename Function, typename... BoundArgs >
   struct bind_obj
   {
      Function func;
      std::tuple< BoundArgs... > bound_args;

      bind_obj( Function&& f, BoundArgs&&... b ) :
         func( f ),
         bound_args( b... )
      {}

      template< typename... CallArgs >
      inline auto operator()( CallArgs... call_args )
      {
         return std::apply( func, bound_args, std::forward< CallArgs >( call_args )... );
      }
   };

   template< typename Function, typename... Args >
   inline bind_obj< Function, Args... > bind_front( Function&& func, Args&&... arg )
   {
      return bind_obj< Function, Args... >(
         std::forward< Function >( func ),
         std::forward< Args >( arg )...
      );
   }

   template< typename Function, typename Stream, typename T >
   inline auto parse_args( Function&& f, Stream& s ) -> decltype(bind_front< Function, Stream, T >)
   {
      T t;
      pack::from_binary( s, t );
      return bind_front( std::forward(f), std::move(t) );
   }

   template< typename Function, typename Stream, typename T, typename... Ts >
   inline auto parse_args( Function&& f, Stream& s ) -> decltype(parse_args< decltype(bind_front< Function, T >), Stream, T >)
   {
      T t;
      pack::from_binary( s, t );
      return parse_args< decltype(bind_front< Function, T >), Stream, Ts... >( bind_front( std::forward(f), std::move(t) ), s );
   }

   template< typename Function, typename Stream >
   inline Function parse_args( Function&& f, Stream& )
   {
      return f;
   }

/*
   template< typename Function, typename... ArgTypes >
   inline void call_thunk_impl( Function&& func, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len, ArgTypes... args )
   {

   }
*/
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

      template< typename ThunkResult, typename ThunkHandler >
      void register_thunk( thunk_id id, ThunkHandler handler )
      {
         _dispatch_map.emplace( id, [handler]( apply_context* ctx, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len )
         {
            pack::detail::input_stringstream instream( arg_ptr, arg_len );
            pack::to_c_str< ThunkResult >( ret_ptr, ret_len, std::invoke( detail::parse_args( detail::bind_front( handler, *ctx ), instream ) ) );
         });
         _pass_through_map.emplace( id, (void*)&handler );
      }

      static const thunk_dispatcher& instance();

   private:
      thunk_dispatcher();

      // These parameters should be refeneces, but std::function doesn't play nicely with references.
      // See https://cboard.cprogramming.com/cplusplus-programming/160098-std-thread-help.html
      typedef std::function< void(apply_context*, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len) > generic_thunk_handler;

      boost::container::flat_map< thunk_id, generic_thunk_handler >  _dispatch_map;
      boost::container::flat_map< thunk_id, void* >                  _pass_through_map;
};

} }
