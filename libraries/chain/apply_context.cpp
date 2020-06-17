#include <koinos/chain/types.hpp>
#include <koinos/chain/apply_context.hpp>

namespace koinos::chain {

void apply_context::set_state_node( state_node_ptr node )
{
   current_state_node = node;
}

state_node_ptr apply_context::get_state_node()const
{
   return current_state_node;
}

void apply_context::clear_state_node()
{
   current_state_node.reset();
}

} // koinos::chain
