#include <koinos/chain/host_api.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/types.hpp>
#include <koinos/chain/execution_context.hpp>
#include <koinos/util/hex.hpp>

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
      _parent_state_node = node->parent();
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

const protocol::transaction* execution_context::get_transaction() const
{
   return _trx;
}

void execution_context::clear_transaction()
{
   _trx = nullptr;
}

void execution_context::set_operation( const protocol::operation& op )
{
   _op = &op;
}

const protocol::operation* execution_context::get_operation() const
{
   return _op;
}

void execution_context::clear_operation()
{
   _op = nullptr;
}

const std::string& execution_context::get_contract_call_args() const
{
   KOINOS_ASSERT( _stack.size() > 1, chain::internal_error_exception, "stack is empty" );
   return _stack[ _stack.size() - 2 ].call_args;
}

uint32_t execution_context::get_contract_entry_point() const
{
   KOINOS_ASSERT( _stack.size() > 1, chain::internal_error_exception, "stack is empty" );
   return _stack[ _stack.size() - 2 ].entry_point;
}

void execution_context::push_frame( stack_frame&& frame )
{
   KOINOS_ASSERT( _stack.size() < execution_context::stack_limit, chain::reversion_exception, "apply context stack overflow" );
   _stack.emplace_back( std::move(frame) );
}

stack_frame execution_context::pop_frame()
{
   KOINOS_ASSERT( _stack.size(), chain::internal_error_exception, "stack is empty" );
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
   KOINOS_ASSERT( _stack.size(), internal_error_exception, "stack empty" );
   _stack[ _stack.size() - 1 ].call_privilege = p;
}

privilege execution_context::get_privilege() const
{
   KOINOS_ASSERT( _stack.size(), internal_error_exception, "stack empty" );
   return _stack[ _stack.size() - 1 ].call_privilege;
}

const std::string& execution_context::get_contract_id() const
{
   for ( auto i = _stack.size() - 1; i >= 0; --i )
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
   auto parent_state_node = get_parent_node();
   KOINOS_ASSERT( parent_state_node, chain::reversion_exception, "cannot build execution context cache without a state node" );

   auto obj = parent_state_node->get_object( state::space::metadata(), state::key::compute_bandwidth_registry );
   KOINOS_ASSERT( obj, chain::reversion_exception, "compute bandwidth registry does not exist" );
   auto compute_registry = util::converter::to< compute_bandwidth_registry >( *obj );

   _cache.compute_bandwidth.emplace();
   for ( const auto& entry : compute_registry.entries() )
      ( *_cache.compute_bandwidth )[ entry.name() ] = entry.compute();
}

void execution_context::build_descriptor_pool()
{
   auto parent_state_node = get_parent_node();
   KOINOS_ASSERT( parent_state_node, chain::reversion_exception, "cannot build execution context cache without a state node" );

   auto pdesc = parent_state_node->get_object( state::space::metadata(), state::key::protocol_descriptor );
   KOINOS_ASSERT( pdesc, chain::reversion_exception, "file descriptor set does not exist" );

   google::protobuf::FileDescriptorSet fdesc;
   KOINOS_ASSERT( fdesc.ParseFromString( *pdesc ), chain::reversion_exception, "file descriptor set is malformed" );

   _cache.descriptor_pool.emplace();
   for ( const auto& fd : fdesc.file() )
      _cache.descriptor_pool->BuildFile( fd );
}

void execution_context::cache_system_call( uint32_t id )
{
   auto parent_state_node = get_parent_node();
   KOINOS_ASSERT( parent_state_node, chain::reversion_exception, "cannot build execution context cache without a state node" );

   if ( _cache.system_call_table.find( id ) != _cache.system_call_table.end() )
      return;

   auto obj = parent_state_node->get_object( state::space::system_call_dispatch(), util::converter::as< std::string >( id ) );

   if ( obj != nullptr )
   {
      auto system_call_target = util::converter::to< protocol::system_call_target >( *obj );

      if ( system_call_target.has_system_call_bundle() )
      {
         const auto& contract_id = system_call_target.system_call_bundle().contract_id();
         auto entry_point        = system_call_target.system_call_bundle().entry_point();
         auto contract_meta      = parent_state_node->get_object( state::space::contract_metadata(), util::converter::as< std::string >( contract_id ) );
         auto contract_bytecode  = parent_state_node->get_object( state::space::contract_bytecode(), util::converter::as< std::string >( contract_id ) );

         KOINOS_ASSERT( contract_meta, invalid_contract_exception, "contract metadata for call id ${id} not found", ("id", id) );
         KOINOS_ASSERT( contract_bytecode, invalid_contract_exception, "contract bytecode for call id ${id} not found", ("id", id) );

         auto success = _cache.system_call_table.emplace(
            id,
            system_call_cache_bundle {
               contract_id,
               *contract_bytecode,
               entry_point,
               util::converter::to< chain::contract_metadata_object >( *contract_meta )
            }
         ).second;
         KOINOS_ASSERT( success, internal_error_exception, "caching system call ${id} failed", ("id", id) );
      }
      else
      {
         auto success = _cache.system_call_table.emplace( id, thunk_cache_bundle { system_call_target.thunk_id(), true } ).second;
         KOINOS_ASSERT( success, internal_error_exception, "caching system call ${id} failed", ("id", id) );
      }
   }
   else
   {
      auto success = _cache.system_call_table.emplace( id, thunk_cache_bundle { id, false } ).second;
      KOINOS_ASSERT( success, internal_error_exception, "caching system call ${id} failed", ("id", id) );
   }
}

void execution_context::build_block_hash_code_cache()
{
   auto parent_state_node = get_parent_node();
   KOINOS_ASSERT( parent_state_node, reversion_exception, "cannot build execution context cache without a state node" );

   auto bhash = parent_state_node->get_object( state::space::metadata(), state::key::block_hash_code );
   KOINOS_ASSERT( bhash, invalid_contract_exception, "block hash code does not exist" );

   _cache.block_hash_code.emplace( crypto::multicodec( util::converter::to< unsigned_varint >( *bhash ).value ) );
}

void execution_context::reset_cache()
{
   _cache.compute_bandwidth.reset();
   _cache.descriptor_pool.reset();
   _cache.system_call_table.clear();
   _cache.block_hash_code.reset();
}

uint64_t execution_context::get_compute_bandwidth( const std::string& thunk_name )
{
   if ( !_cache.compute_bandwidth )
      build_compute_registry_cache();

   auto itr = _cache.compute_bandwidth->find( thunk_name );

   KOINOS_ASSERT( itr != _cache.compute_bandwidth->end(), reversion_exception, "unable to find compute bandwidth for ${t}", ("t", thunk_name) );

   return itr->second;
}

const google::protobuf::DescriptorPool& execution_context::descriptor_pool()
{
   if ( !_cache.descriptor_pool )
      build_descriptor_pool();

   return *_cache.descriptor_pool;
}

const execution_result& execution_context::system_call( uint32_t id, const std::string& args )
{
   try
   {
      cache_system_call( id );

      auto itr = _cache.system_call_table.find( id );
      KOINOS_ASSERT( itr != _cache.system_call_table.end(), reversion_exception, "unable to find call id ${id} in system call cache", ("id", id) );

      const auto* call_bundle = std::get_if< system_call_cache_bundle >( &itr->second );
      KOINOS_ASSERT( call_bundle, reversion_exception, "system call ${id} is implemented via thunk", ("id", id) );

      with_stack_frame(
         *this,
         stack_frame {
            .contract_id = call_bundle->contract_id,
            .call_privilege = call_bundle->contract_metadata.system() ? privilege::kernel_mode : privilege::user_mode,
            .call_args = args,
            .entry_point = call_bundle->entry_point
         },
         [&]
         {
            chain::host_api hapi( *this );
            get_backend()->run( hapi, call_bundle->contract_bytecode, call_bundle->contract_metadata.hash() );
         }
      );
   }
   catch ( const success_exception& ) {}

   return get_result();
}

bool execution_context::system_call_exists( uint32_t id )
{
   cache_system_call( id );

   auto itr = _cache.system_call_table.find( id );
   if ( itr == _cache.system_call_table.end() )
      return false;

   return std::get_if< system_call_cache_bundle >( &itr->second ) != nullptr;
}

uint32_t execution_context::thunk_translation( uint32_t id )
{
   cache_system_call( id );

   auto itr = _cache.system_call_table.find( id );
   KOINOS_ASSERT( itr != _cache.system_call_table.end(), reversion_exception, "unable to find call id ${id} in system call cache", ("id", id) );

   const auto* thunk_bundle = std::get_if< thunk_cache_bundle >( &itr->second );
   KOINOS_ASSERT( thunk_bundle, reversion_exception, "system call ${id} is implemented via contract override", ("id", id) );

   if ( thunk_bundle->is_override )
   {
      return thunk_bundle->thunk_id;
   }

   KOINOS_ASSERT( thunk_bundle->thunk_id == id, internal_error_exception, "non-override cached thunk id ${cached} does not match id ${id}", ("cached", thunk_bundle->thunk_id)("id", id) );
   KOINOS_ASSERT( thunk_dispatcher::instance().thunk_is_genesis( id ), unknown_thunk_exception, "thunk ${id} is not enabled", ("id", id) );
   return id;
}

const crypto::multicodec& execution_context::block_hash_code()
{
   if ( !_cache.block_hash_code )
      build_block_hash_code_cache();

   return *_cache.block_hash_code;
}

void execution_context::set_result( const execution_result& r )
{
   _result = r;
}

void execution_context::set_result( execution_result&& r )
{
   _result = std::move( r );
}

const execution_result& execution_context::get_result()
{
   return _result;
}

} // koinos::chain
