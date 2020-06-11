
#include <koinos/chain/thunks.hpp>

namespace koinos { namespace chain { namespace thunk {

// When defining a thunk, define it here
void hello( apply_context& ctx, hello_thunk_ret& ret, const hello_thunk_args& arg )
{
   ret.c = arg.a + arg.b;
   ret.d = arg.a - arg.b;
}

} } }
