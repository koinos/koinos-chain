#include <koinos/chain/host_api.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/chain/types.hpp>
#include <koinos/chain/execution_context.hpp>

namespace koinos::chain {

namespace constants {
   const std::string system = "";
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
   KOINOS_ASSERT( _trx != nullptr, unexpected_access, "transaction does not exist" );
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
   if ( _stack.size() > 1 )
      return _stack[ _stack.size() - 2 ].contract_id;

   return constants::system;
}

privilege execution_context::get_caller_privilege() const
{
   if ( _stack.size() > 1 )
      return _stack[ _stack.size() - 2 ].call_privilege;

   return privilege::kernel_mode;
}

uint32_t execution_context::get_caller_entry_point() const
{
   if ( _stack.size() > 1 )
      return _stack[ _stack.size() - 2 ].entry_point;

   return 0;
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

const std::string& execution_context::get_contract_id() const
{
   for ( int32_t i = _stack.size() - 1; i >= 0; --i )
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

chronicler& execution_context::chronicler()
{
   return _chronicler;
}

std::shared_ptr< session > execution_context::make_session( uint64_t rc )
{
   auto session = std::make_shared< chain::session >( rc );
   resource_meter().set_session( session );
   chronicler().set_session( session );
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

void execution_context::build_compute_registry_cache()
{
   auto obj = _current_state_node->get_object( state::space::metadata(), state::key::compute_bandwidth_registry );
   KOINOS_ASSERT( obj, unexpected_state, "compute bandwidth registry does not exist" );
   auto compute_registry = util::converter::to< compute_bandwidth_registry >( *obj );

   _cache.compute_bandwidth.clear();
   for ( const auto& entry : compute_registry.entries() )
      _cache.compute_bandwidth[ entry.name() ] = entry.compute();
}

void execution_context::build_descriptor_pool()
{
   auto pdesc = _current_state_node->get_object( state::space::metadata(), state::key::protocol_descriptor );
   KOINOS_ASSERT( pdesc, unexpected_state, "file descriptor set does not exist" );

   google::protobuf::FileDescriptorSet fdesc;
   KOINOS_ASSERT( fdesc.ParseFromString( *pdesc ), unexpected_state, "file descriptor set is malformed" );

   _cache.descriptor_pool = std::make_unique< google::protobuf::DescriptorPool >();
   for ( const auto& fd : fdesc.file() )
      _cache.descriptor_pool->BuildFile( fd );
}

void execution_context::build_system_call_cache()
{
   _cache.system_call.clear();
   state_db::object_key next = std::string{};
   for (;;)
   {
      auto obj = _current_state_node->get_next_object( state::space::system_call_dispatch(), next );
      if ( obj.first == nullptr )
         break;

      next = obj.second;

      auto system_call_target = util::converter::to< protocol::system_call_target >( *obj.first );

      auto call_id = util::converter::to< uint32_t >( obj.second );
      if ( system_call_target.has_system_call_bundle() )
      {
         auto contract_id       = system_call_target.system_call_bundle().contract_id();
         auto entry_point       = system_call_target.system_call_bundle().entry_point();
         auto contract_meta     = _current_state_node->get_object( state::space::contract_metadata(), util::converter::as< std::string >( contract_id ) );
         auto contract_bytecode = _current_state_node->get_object( state::space::contract_bytecode(), util::converter::as< std::string >( contract_id ) );

         KOINOS_ASSERT( contract_meta, unexpected_state, "contract metadata for call id ${id} not found", ("id", call_id) );
         KOINOS_ASSERT( contract_bytecode, unexpected_state, "contract bytecode for call id ${id} not found", ("id", call_id) );

         _cache.system_call[ call_id ] = std::make_tuple( contract_id, *contract_bytecode, entry_point, util::converter::to< chain::contract_metadata_object >( *contract_meta ) );
      }
      else
      {
         KOINOS_ASSERT( system_call_target.has_thunk_id(), unexpected_state, "expected thunk id for call id ${id}", ("id", call_id) );
         _cache.thunk[ call_id ] = system_call_target.thunk_id();
      }
   }
}

void execution_context::build_cache()
{
   KOINOS_ASSERT( _current_state_node, unexpected_state, "cannot rebuild execution context cache without a state node" );

   build_compute_registry_cache();
   build_descriptor_pool();
   build_system_call_cache();
}

uint64_t execution_context::get_compute_bandwidth( const std::string& thunk_name ) const
{
   auto iter = _cache.compute_bandwidth.find( thunk_name );
   KOINOS_ASSERT( iter != _cache.compute_bandwidth.end(), unexpected_state, "unable to find compute bandwidth for ${t}", ("t", thunk_name) );
   return iter->second;
}

const google::protobuf::DescriptorPool& execution_context::descriptor_pool() const
{
   KOINOS_ASSERT( _cache.descriptor_pool, unexpected_state, "descriptor pool has not been built" );
   return *_cache.descriptor_pool;
}

std::string execution_context::system_call( uint32_t id, const std::string& args )
{
   auto iter = _cache.system_call.find( id );
   KOINOS_ASSERT( iter != _cache.system_call.end(), unexpected_state, "unable to find call id ${id} in system call cache", ("id", id) );

   const auto& cid      = std::get< 0 >( iter->second );
   const auto& bytecode = std::get< 1 >( iter->second );
   auto entry_point     = std::get< 2 >( iter->second );
   const auto& meta     = std::get< 3 >( iter->second );

   push_frame( stack_frame{
      .contract_id = cid,
      .call_privilege = meta.system() ? privilege::kernel_mode : privilege::user_mode,
      .call_args = args,
      .entry_point = entry_point
   } );

   try
   {
      chain::host_api hapi( *this );
      get_backend()->run( hapi, bytecode, meta.hash() );
   }
   catch( const exit_success& ) {}
   catch( ... ) {
      pop_frame();
      throw;
   }

   return pop_frame().call_return;
}

bool execution_context::system_call_exists( uint32_t id ) const
{
   return _cache.system_call.find( id ) != _cache.system_call.end();
}

uint32_t execution_context::thunk_translation( uint32_t id ) const
{
   auto iter = _cache.thunk.find( id );
   if ( iter != _cache.thunk.end() )
      *iter;
   return id;
}

} // koinos::chain
