#pragma once

#include <koinos/pack/classes.hpp>

namespace koinos { namespace chain {
class apply_context;

namespace thunk {

void hello( apply_context& ctx, hello_thunk_ret& ret, const hello_thunk_args& arg );

} } }
