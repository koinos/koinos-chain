#include <koinos/chain/types.hpp>
#include <koinos/chain/apply_context.hpp>

namespace koinos::chain {

apply_context::apply_context( std::shared_ptr< vm_manager::vm_backend > vm_backend ) :
   _vm_backend( vm_backend ) {}

std::shared_ptr< vm_manager::vm_backend > apply_context::get_backend() const
{
   return _vm_backend;
}

void apply_context::console_append( const std::string& val )
{
   _pending_console_output += val;
}

std::string apply_context::get_pending_console_output()
{
   std::string buf = _pending_console_output;
   _pending_console_output.clear();
   return buf;
}

void apply_context::set_state_node( abstract_state_node_ptr node, abstract_state_node_ptr parent )
{
   _current_state_node = node;
   if ( parent )
      _parent_state_node = parent;
   else if ( _current_state_node )
      _parent_state_node = node->get_parent();
   else
      _parent_state_node.reset();
}

abstract_state_node_ptr apply_context::get_state_node()const
{
   return _current_state_node;
}

abstract_state_node_ptr apply_context::get_parent_node()const
{
   // This handles the genesis case
   return _parent_state_node ? _parent_state_node : _current_state_node;
}

void apply_context::clear_state_node()
{
   _current_state_node.reset();
   _parent_state_node.reset();
}

void apply_context::set_block( const protocol::block& block )
{
   _block = &block;
}

const protocol::block* apply_context::get_block()const
{
   return _block;
}

void apply_context::clear_block()
{
   _block = nullptr;
}

void apply_context::set_transaction( const protocol::transaction& trx )
{
   _trx = &trx;
}

const protocol::transaction& apply_context::get_transaction()const
{
   return *_trx;
}

void apply_context::clear_transaction()
{
   _trx = nullptr;
}

const std::vector< std::byte >& apply_context::get_contract_call_args() const
{
   KOINOS_ASSERT( _stack.size() > 1, stack_exception, "stack is empty" );
   return _stack[ _stack.size() - 2 ].call_args;
}

std::vector< std::byte > apply_context::get_contract_return() const
{
   KOINOS_ASSERT( _stack.size() > 1, stack_exception, "stack is empty" );
   return _stack[ _stack.size() - 2 ].call_return;
}

uint32_t apply_context::get_contract_entry_point() const
{
   KOINOS_ASSERT( _stack.size() > 1, stack_exception, "stack is empty" );
   return _stack[ _stack.size() - 2 ].entry_point;
}

void apply_context::set_contract_return( const std::vector< std::byte >& ret )
{
   KOINOS_ASSERT( _stack.size() > 1, stack_exception, "stack is empty" );
   _stack[ _stack.size() - 2 ].call_return = ret;
}

void apply_context::set_key_authority( const crypto::public_key& key )
{
   _key_auth = key;
}

void apply_context::clear_authority()
{
   _key_auth.reset();
}

void apply_context::push_frame( stack_frame&& frame )
{
   KOINOS_ASSERT( _stack.size() < APPLY_CONTEXT_STACK_LIMIT, stack_overflow, "apply context stack overflow" );
   _stack.emplace_back( std::move(frame) );
}

stack_frame apply_context::pop_frame()
{
   KOINOS_ASSERT( _stack.size(), stack_exception, "stack is empty" );
   auto frame = _stack[ _stack.size() - 1 ];
   _stack.pop_back();
   return frame;
}

const std::vector< std::byte >& apply_context::get_caller()const
{
   KOINOS_ASSERT( _stack.size() > 1, stack_exception, "stack has no calling frame" );
   return _stack[ _stack.size() - 2 ].call;
}

privilege apply_context::get_caller_privilege()const
{
   KOINOS_ASSERT( _stack.size() > 1, stack_exception, "stack has no calling frame" );
   return _stack[ _stack.size() - 2 ].call_privilege;
}

void apply_context::set_privilege( privilege p )
{
   KOINOS_ASSERT( _stack.size() , stack_exception, "stack has no calling frame" );
   _stack[ _stack.size() - 1 ].call_privilege = p;
}

privilege apply_context::get_privilege()const
{
   KOINOS_ASSERT( _stack.size() , stack_exception, "stack has no calling frame" );
   return _stack[ _stack.size() - 1 ].call_privilege;
}

void apply_context::set_in_user_code( bool is_in_user_code )
{
   _is_in_user_code = is_in_user_code;
}

bool apply_context::is_in_user_code()const
{
   return _is_in_user_code;
}

void apply_context::set_read_only( bool ro )
{
   _read_only = ro;
}

bool apply_context::is_read_only()const
{
   return _read_only;
}

void apply_context::reset_meter_ticks( int64_t meter_ticks )
{
   KOINOS_ASSERT( meter_ticks >= 0, invalid_meter_ticks, "cannot set negative meter ticks" );
   _meter_ticks = meter_ticks;
   _start_meter_ticks = meter_ticks;
}

void apply_context::set_meter_ticks( int64_t meter_ticks )
{
   KOINOS_ASSERT( meter_ticks <= _meter_ticks, invalid_meter_ticks, "cannot add meter ticks" );
   _meter_ticks = meter_ticks;
}

void apply_context::use_meter_ticks( int64_t meter_ticks )
{
   KOINOS_ASSERT( meter_ticks >= 0, invalid_meter_ticks, "cannot consume negative meter ticks" );
   _meter_ticks -= meter_ticks;
}

int64_t apply_context::get_meter_ticks()
{
   return _meter_ticks;
}

int64_t apply_context::get_used_meter_ticks()
{
   return _start_meter_ticks - _meter_ticks;
}

} // koinos::chain
