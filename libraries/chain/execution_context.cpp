#include <koinos/chain/types.hpp>
#include <koinos/chain/execution_context.hpp>

namespace koinos::chain {

namespace constants {
   const std::string system = "system";
}

execution_context::execution_context( std::shared_ptr< vm_manager::vm_backend > vm_backend, chain::intent i ) :
   _vm_backend( vm_backend )
{
   set_intent( i );
}

std::shared_ptr< vm_manager::vm_backend > execution_context::get_backend() const
{
   return _vm_backend;
}

void execution_context::console_append( const std::string& val )
{
   _pending_console_output += val;
}

std::string execution_context::get_pending_console_output()
{
   std::string buf = _pending_console_output;
   _pending_console_output.clear();
   return buf;
}

void execution_context::set_state_node( abstract_state_node_ptr node, abstract_state_node_ptr parent )
{
   _current_state_node = node;
   if ( parent )
      _parent_state_node = parent;
   else if ( _current_state_node )
      _parent_state_node = node->get_parent();
   else
      _parent_state_node.reset();
}

abstract_state_node_ptr execution_context::get_state_node() const
{
   return _current_state_node;
}

abstract_state_node_ptr execution_context::get_parent_node() const
{
   // This handles the genesis case
   return _parent_state_node ? _parent_state_node : _current_state_node;
}

void execution_context::clear_state_node()
{
   _current_state_node.reset();
   _parent_state_node.reset();
}

void execution_context::set_block( const protocol::block& block )
{
   _block = &block;
}

const protocol::block* execution_context::get_block() const
{
   return _block;
}

void execution_context::clear_block()
{
   _block = nullptr;
}

void execution_context::set_transaction( const protocol::transaction& trx )
{
   _trx = &trx;
}

const protocol::transaction& execution_context::get_transaction() const
{
   KOINOS_ASSERT( _trx != nullptr, unexpected_access, "attempting to dereference a null pointer" );
   return *_trx;
}

void execution_context::clear_transaction()
{
   _trx = nullptr;
}

const std::string& execution_context::get_contract_call_args() const
{
   KOINOS_ASSERT( _stack.size() > 1, stack_exception, "stack is empty" );
   return _stack[ _stack.size() - 2 ].call_args;
}

std::string execution_context::get_contract_return() const
{
   KOINOS_ASSERT( _stack.size() > 1, stack_exception, "stack is empty" );
   return _stack[ _stack.size() - 2 ].call_return;
}

uint32_t execution_context::get_contract_entry_point() const
{
   KOINOS_ASSERT( _stack.size() > 1, stack_exception, "stack is empty" );
   return _stack[ _stack.size() - 2 ].entry_point;
}

void execution_context::set_contract_return( const std::string& ret )
{
   KOINOS_ASSERT( _stack.size() > 1, stack_exception, "stack is empty" );
   _stack[ _stack.size() - 2 ].call_return = ret;
}

void execution_context::set_key_authority( const crypto::public_key& key )
{
   _key_auth = key;
}

void execution_context::clear_authority()
{
   _key_auth.reset();
}

void execution_context::push_frame( stack_frame&& frame )
{
   KOINOS_ASSERT( _stack.size() < execution_context::stack_limit, stack_overflow, "apply context stack overflow" );
   _stack.emplace_back( std::move(frame) );
}

stack_frame execution_context::pop_frame()
{
   KOINOS_ASSERT( _stack.size(), stack_exception, "stack is empty" );
   auto frame = _stack[ _stack.size() - 1 ];
   _stack.pop_back();
   return frame;
}

const std::string& execution_context::get_caller() const
{
   for ( size_t i = _stack.size() - 1; i >= 0; --i )
   {
      if ( !_stack[ i ].system )
         return _stack[ i ].contract_id;
   }
   return constants::system;
}

privilege execution_context::get_caller_privilege() const
{
   for ( size_t i = _stack.size() - 1; i >= 0; --i )
   {
      if ( !_stack[ i ].system )
         return _stack[ i ].call_privilege;
   }

   return privilege::kernel_mode;
}

void execution_context::set_privilege( privilege p )
{
   KOINOS_ASSERT( _stack.size() , stack_exception, "stack empty" );
   _stack[ _stack.size() - 1 ].call_privilege = p;
}

privilege execution_context::get_privilege() const
{
   KOINOS_ASSERT( _stack.size() , stack_exception, "stack empty" );
   return _stack[ _stack.size() - 1 ].call_privilege;
}

bool execution_context::get_system() const
{
   KOINOS_ASSERT( _stack.size() , stack_exception, "stack empty" );
   return _stack[ _stack.size() - 1 ].system;
}

const std::string& execution_context::get_contract_id() const
{
   for ( size_t i = _stack.size() - 1; i >= 0; --i )
   {
      if ( _stack[ i ].contract_id.size() )
         return _stack[ i ].contract_id;
   }
   return constants::system;
}

bool execution_context::read_only() const
{
   return _intent == intent::read_only;
}

resource_meter& execution_context::resource_meter()
{
   return _resource_meter;
}

event_recorder& execution_context::event_recorder()
{
   return _event_recorder;
}

std::shared_ptr< session > execution_context::make_session( uint64_t rc )
{
   auto session = std::make_shared< chain::session >( rc );
   resource_meter().set_session( session );
   event_recorder().set_session( session );
   return session;
}

chain::receipt& execution_context::receipt()
{
   return _receipt;
}

void execution_context::set_intent( chain::intent i )
{
   _intent = i;
}

chain::intent execution_context::intent() const
{
   return _intent;
}

} // koinos::chain
