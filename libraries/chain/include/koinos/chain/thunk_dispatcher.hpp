#pragma once

#include <cstdint>

#include <koinos/chain/thunks.hpp>
#include <koinos/exception.hpp>

namespace koinos { namespace protocol {
struct vl_blob;
} }

namespace koinos { namespace chain {

DECLARE_KOINOS_EXCEPTION( unknown_thunk );

class apply_context;

namespace detail {
class thunk_dispatcher_impl;
}

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
      virtual ~thunk_dispatcher();

      virtual void call_thunk( thunk_id id, apply_context& ctx, protocol::vl_blob& ret, const protocol::vl_blob& args ) = 0;

      static thunk_dispatcher& instance();

   private:
      thunk_dispatcher();

      friend class detail::thunk_dispatcher_impl;
};

} }
