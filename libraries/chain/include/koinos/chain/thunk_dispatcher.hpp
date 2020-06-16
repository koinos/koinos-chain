#pragma once

#include <cstdint>

#include <koinos/chain/thunks.hpp>
#include <koinos/exception.hpp>

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

      template< typename ThunkRet, typename ThunkHandler, typename... ThunkArgs >
      ThunkRet call_thunk( uint32_t id, apply_context& ctx, ThunkArgs... args )
      {
         auto it = _pass_through_map.find( id );
         KOINOS_ASSERT( it != _pass_through_map.end(), unknown_thunk, "Thunk ${id} not found", ("id", id) );
         return (*(ThunkHandler*)(it->second))( ctx, args... );
      }

      template< typename ThunkHandler, typename... ThunkArgs >
      void call_thunk( uint32_t id, apply_context& ctx, ThunkArgs... args )
      {
         auto it = _pass_through_map.find( id );
         KOINOS_ASSERT( it != _pass_through_map.end(), unknown_thunk, "Thunk ${id} not found", ("id", id) );
         (*(ThunkHandler*)(it->second))( ctx, args... );
      }

      static thunk_dispatcher& instance();

   private:
      thunk_dispatcher();

      // These parameters should be refeneces, but std::function doesn't play nicely with references.
      // See https://cboard.cprogramming.com/cplusplus-programming/160098-std-thread-help.html
      typedef std::function< void(apply_context*, protocol::vl_blob*, const protocol::vl_blob*) > generic_thunk_handler;

      boost::container::flat_map< thunk_id, generic_thunk_handler >  _dispatch_map;
      boost::container::flat_map< thunk_id, void* >                  _pass_through_map;
};

} }
