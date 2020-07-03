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

void apply_context::set_contract_call_args( const variable_blob& args )
{
   contract_call_args = args;
}

const variable_blob& apply_context::get_contract_call_args()
{
   return contract_call_args;
}

types::variable_blob apply_context::get_contract_return()
{
   auto ret = contract_return;
   contract_return = types::variable_blob();
   return ret;
}

void apply_context::set_contract_return( const types::variable_blob& ret )
{
   contract_return = ret;
}

} // koinos::chain
