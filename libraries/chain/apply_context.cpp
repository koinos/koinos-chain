#include <koinos/chain/types.hpp>
#include <koinos/chain/apply_context.hpp>

namespace koinos::chain {

apply_context::apply_context( system_call_table& _sct ) :
   syscalls( _sct )
{}

} // koinos::chain
