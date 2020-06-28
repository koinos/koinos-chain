#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/register_thunks.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/thunks.hpp>
#include <koinos/chain/system_calls.hpp>

namespace koinos::chain {

using namespace koinos::types;
using namespace koinos::types::thunks;

void register_thunks( thunk_dispatcher& td )
{
   REGISTER_THUNKS( td,
   (prints)

   (verify_block_header)

   (apply_block)
   (apply_transaction)
   (apply_reserved_operation)
   (apply_upload_contract_operation)
   (apply_execute_contract_operation)
   (apply_set_system_call_operation)

   (db_put_object)
   (db_get_object)
   (db_get_next_object)
   (db_get_prev_object)

   (get_contract_args_size)
   (get_contract_args)
   )
}

namespace thunk {

THUNK_DEFINE( void, prints, ((const std::string&) str) )
{
   context.console_append( str );
}

THUNK_DEFINE( bool, verify_block_header, ((const crypto::recoverable_signature&) sig, (const crypto::multihash_type&) digest) )
{
   return crypto::public_key::from_base58( "5evxVPukp6bUdGNX8XUMD9e2J59j9PjqAVw2xYNw5xrdQPRRT8" ) == crypto::public_key::recover( sig, digest );
}

THUNK_DEFINE( void, apply_block, ((const protocol::active_block_data&) b) )
{
   for ( auto& t : b.transactions )
   {
      apply_transaction( context, pack::from_variable_blob< protocol::transaction_type >( t ) );
   }
}

THUNK_DEFINE( void, apply_transaction, ((const protocol::transaction_type&) t) )
{
   using namespace koinos::types::protocol;

   for ( auto& o : t.operations )
   {
      std::visit( koinos::overloaded {
         [&]( const nop_operation& op ) { /* intentional fallthrough */ },
         [&]( const reserved_operation& op )
         {
            apply_reserved_operation( context, op );
         },
         [&]( const create_system_contract_operation& op )
         {
            apply_upload_contract_operation( context, op );
         },
         [&]( const contract_call_operation& op )
         {
            apply_execute_contract_operation( context, op );
         },
         [&]( const protocol::set_system_call_operation& op )
         {
            apply_set_system_call_operation( context, op );
         },
      }, pack::from_variable_blob< operation >( o ) );
   }
}

THUNK_DEFINE( void, apply_reserved_operation, ((const protocol::reserved_operation&) o) )
{
   KOINOS_THROW( reserved_operation_exception, "Unable to apply reserved operation" );
}

THUNK_DEFINE( void, apply_upload_contract_operation, ((const protocol::create_system_contract_operation&) o) )
{
   // Contract id is a ripemd160. It needs to be copied in to a uint256_t
   types::uint256_t contract_id = pack::from_fixed_blob< types::uint160_t >( o.contract_id );
   db_put_object( context, CONTRACT_SPACE_ID, contract_id, o.bytecode );
}

THUNK_DEFINE( void, apply_execute_contract_operation, ((const protocol::contract_call_operation&) o) )
{
   types::uint256_t contract_key = pack::from_fixed_blob< types::uint160_t >( o.contract_id );
   auto bytecode = db_get_object( context, CONTRACT_SPACE_ID, contract_key );
   wasm_allocator_type wa;

   wasm_code_ptr bytecode_ptr( (uint8_t*)bytecode.data(), bytecode.size() );
   backend_type backend( bytecode_ptr, bytecode_ptr.bounds(), registrar_type{} );

   backend.set_wasm_allocator( &wa );
   backend.initialize();

   context.set_contract_call_args( o.args );
   backend( &context, "env", "apply", (uint64_t)0, (uint64_t)0, (uint64_t)0 );
}

THUNK_DEFINE( void, apply_set_system_call_operation, ((const protocol::set_system_call_operation&) o) )
{
   // Ensure contract exists.
   types::uint256_t contract_key = pack::from_fixed_blob< types::uint160_t >( o.contract_id );
   auto contract = db_get_object(context, CONTRACT_SPACE_ID, contract_key);
   KOINOS_ASSERT( contract.size(), invalid_contract, "Contract does not exist" );

   // Store the system call bundle in the database
   types::system::system_call_bundle bundle;
   bundle.contract_id = o.contract_id;
   bundle.entry_point = o.entry_point;
   types::system::system_call_target sys_call = bundle;
   db_put_object( context, SYS_CALL_DISPATCH_TABLE_SPACE_ID, o.call_id, pack::to_variable_blob( sys_call ) );
}

THUNK_DEFINE( bool, db_put_object, ((const statedb::object_space&) space, (const statedb::object_key&) key, (const variable_blob&) obj) )
{
   auto state = context.get_state_node();
   KOINOS_ASSERT( state, database_exception, "Current state node does not exist" );
   statedb::put_object_args put_args;
   put_args.space = space;
   put_args.key = key;
   put_args.buf = obj.data();
   put_args.object_size = obj.size();

   statedb::put_object_result put_res;
   state->put_object( put_res, put_args );

   return put_res.object_existed;
}

THUNK_DEFINE( variable_blob, db_get_object, ((const statedb::object_space&) space, (const statedb::object_key&) key, (int32_t) object_size_hint) )
{
   auto state = context.get_state_node();
   KOINOS_ASSERT( state, database_exception, "Current state node does not exist" );

   statedb::get_object_args get_args;
   get_args.space = space;
   get_args.key = key;
   get_args.buf_size = object_size_hint > 0 ? object_size_hint : STATE_DB_MAX_OBJECT_SIZE;

   variable_blob object_buffer;
   object_buffer.resize( get_args.buf_size );
   get_args.buf = object_buffer.data();

   statedb::get_object_result get_res;
   state->get_object( get_res, get_args );

   if( get_res.key == get_args.key && get_res.size > 0 )
      object_buffer.resize( get_res.size );
   else
      object_buffer.clear();
   return object_buffer;
}

THUNK_DEFINE( variable_blob, db_get_next_object, ((const statedb::object_space&) space, (const statedb::object_key&) key, (int32_t) object_size_hint) )
{
   auto state = context.get_state_node();
   KOINOS_ASSERT( state, database_exception, "Current state node does not exist" );
   statedb::get_object_args get_args;
   get_args.space = space;
   get_args.key = key;
   get_args.buf_size = object_size_hint > 0 ? object_size_hint : STATE_DB_MAX_OBJECT_SIZE;

   variable_blob object_buffer;
   object_buffer.resize( get_args.buf_size );
   get_args.buf = object_buffer.data();

   statedb::get_object_result get_res;
   state->get_next_object( get_res, get_args );

   if( get_res.size > 0 )
      object_buffer.resize( get_res.size );
   else
      object_buffer.clear();

   return object_buffer;
}

THUNK_DEFINE( variable_blob, db_get_prev_object, ((const statedb::object_space&) space, (const statedb::object_key&) key, (int32_t) object_size_hint) )
{
   auto state = context.get_state_node();
   KOINOS_ASSERT( state, database_exception, "Current state node does not exist" );
   statedb::get_object_args get_args;
   get_args.space = space;
   get_args.key = key;
   get_args.buf_size = object_size_hint > 0 ? object_size_hint : STATE_DB_MAX_OBJECT_SIZE;

   variable_blob object_buffer;
   object_buffer.resize( get_args.buf_size );
   get_args.buf = object_buffer.data();

   statedb::get_object_result get_res;
   state->get_prev_object( get_res, get_args );

   if( get_res.size > 0 )
      object_buffer.resize( get_res.size );
   else
      object_buffer.clear();

   return object_buffer;
}

THUNK_DEFINE_VOID( uint32_t, get_contract_args_size )
{
   return (uint32_t)context.get_contract_call_args().size();
}

THUNK_DEFINE_VOID( variable_blob, get_contract_args )
{
   return context.get_contract_call_args();
}

} } // koinos::chain::thunk
