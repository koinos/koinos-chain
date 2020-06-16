#include <koinos/pack/rt/pack_fwd.hpp>

#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/xcalls.hpp>

#include <koinos/pack/rt/string.hpp>

#include <koinos/log.hpp>
#include <koinos/util.hpp>

std::string hex_string( const char* d, size_t len )
{
   static const auto hex = "0123456789ABCDEF";

   std::stringstream ss;

   for( int i = 0; i < len; i++)
      ss << hex[(*(d + i) & 0xF0) >> 4] << hex[*(d + i) & 0x0F];

   return ss.str();
}

namespace koinos::chain {

// System API Definitions
//
// Using the SYSTEM_CALL_DEFINE macro will define the public facing system call for you. It will check the
// system_call_table class to see if an override exists and call it. If there is no system call override for
// the given system call it will call the native implement that you define.
//
// The native implementation should check whether or not we are in kernel mode and throw if we are not. This
// will prevent the user mode contract from circumventing the public interface. Use the provided macro
// SYSTEM_CALL_ENFORCE_KERNEL_MODE();

system_api::system_api( apply_context& _ctx ) : context( _ctx ) {}

SYSTEM_CALL_DEFINE( void, prints, ((null_terminated_ptr) str) )
{
   context.console_append( static_cast< const char* >(str) );
}

void system_api::invoke_thunk( uint32_t tid, array_ptr< char > ret_ptr, uint32_t ret_len, array_ptr< const char > arg_ptr, uint32_t arg_len )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, "cannot be called directly from user mode" );
   thunk_dispatcher::instance().call_thunk( thunk_id(tid), context, ret_ptr, ret_len, arg_ptr, arg_len );
}

void system_api::invoke_xcall( uint32_t xid, array_ptr< char > ret_ptr, uint32_t ret_len, array_ptr< const char > arg_ptr, uint32_t arg_len )
{
   using protocol::thunk_id_type;
   using protocol::contract_id_type;

   // TODO Do we need to invoke serialization here?
   statedb::object_key key = xid;

   vl_blob vl_target =
      db_get_object_thunk( context, XCALL_DISPATCH_TABLE_SPACE_ID, key, XCALL_DISPATCH_TABLE_OBJECT_MAX_SIZE );

   if( vl_target.data.size() == 0 )
   {
      vl_target = get_default_xcall_entry( xid );
      KOINOS_ASSERT( vl_target.data.size() > 0,
         unknown_xcall,
         "xcall table dispatch entry ${xid} does not exist",
         ("xid", xid)
         );
   }

   protocol::xcall_target target;

   koinos::pack::from_vl_blob( vl_target, target );

   std::visit(
      koinos::overloaded{
         [&]( thunk_id_type& tid ) {
            thunk_dispatcher::instance().call_thunk( tid, context, ret_ptr, ret_len, arg_ptr, arg_len );
         },
         [&]( contract_id_type& cid ) {
#pragma message( "TODO:  Invoke smart contract xcall handler" )
         },
         [&]( auto& ) {
            KOINOS_THROW( unknown_xcall, "xcall table dispatch entry ${xid} has unimplemented type ${tag}",
               ("xid", xid)("tag", target.index()) );
         } }, target );
}

SYSTEM_CALL_DEFINE( bool, verify_block_header, ((const crypto::recoverable_signature&) sig, (const crypto::multihash_type&) digest) )
{
   return crypto::public_key::from_base58( "5evxVPukp6bUdGNX8XUMD9e2J59j9PjqAVw2xYNw5xrdQPRRT8" ) == crypto::public_key::recover( sig, digest );
}

SYSTEM_CALL_DEFINE( void, apply_block, ((const protocol::active_block_data&) b) )
{
   for ( auto& t : b.transactions )
   {
      apply_transaction( context, pack::from_vl_blob< protocol::transaction_type >( t ) );
   }
}

SYSTEM_CALL_DEFINE( void, apply_transaction, ((const protocol::transaction_type&) t) )
{
   for ( auto& o : t.operations )
   {
      std::visit( koinos::overloaded {
         [&]( const protocol::reserved_operation& op ) { /* intentional fallthrough */ },
         [&]( const protocol::nop_operation& op ) { /* intentional fallthrough */ },
         [&]( const protocol::create_system_contract_operation& op )
         {
            apply_upload_contract_operation( context, op );
         },
         [&]( const protocol::contract_call_operation& op )
         {
            apply_execute_contract_operation( context, op );
         },
      }, pack::from_vl_blob< pack::operation >( o ) );
   }
}

SYSTEM_CALL_DEFINE( void, apply_upload_contract_operation, ((const protocol::create_system_contract_operation&) o) )
{
   // Contract id is a ripemd160. It needs to be copied in to a uint256_t
   protocol::uint256_t contract_id = pack::from_fl_blob< protocol::uint160_t >( o.contract_id );
   db_put_object( context, 0, contract_id, o.bytecode );
}

SYSTEM_CALL_DEFINE( void, apply_execute_contract_operation, ((const protocol::contract_call_operation&) o) )
{
   protocol::uint256_t contract_key = pack::from_fl_blob< protocol::uint160_t >( o.contract_id );
   auto bytecode = db_get_object( context, 0, contract_key );
   wasm_allocator_type wa;

   wasm_code_ptr bytecode_ptr( (uint8_t*)bytecode.data.data(), bytecode.data.size() );
   backend_type backend( bytecode_ptr, bytecode_ptr.bounds(), registrar_type{} );

   backend.set_wasm_allocator( &wa );
   backend.initialize();

   backend( &context, "env", "apply", (uint64_t)0, (uint64_t)0, (uint64_t)0 );
}

SYSTEM_CALL_DEFINE( bool, db_put_object, ((const statedb::object_space&) space, (const statedb::object_key&) key, (const vl_blob&) obj) )
{
   auto state = context.get_state_node();
   KOINOS_ASSERT( state, database_exception, "Current state node does not exist", () );
   statedb::put_object_args put_args;
   put_args.space = space;
   put_args.key = key;
   put_args.buf = obj.data.data();
   put_args.object_size = obj.data.size();

   statedb::put_object_result put_res;
   state->put_object( put_res, put_args );

   return put_res.object_existed;
}

SYSTEM_CALL_DEFINE( vl_blob, db_get_object, ((const statedb::object_space&) space, (const statedb::object_key&) key, (int32_t) object_size_hint) )
{
   auto state = context.get_state_node();
   KOINOS_ASSERT( state, database_exception, "Current state node does not exist", () );

   statedb::get_object_args get_args;
   get_args.space = space;
   get_args.key = key;
   get_args.buf_size = object_size_hint > 0 ? object_size_hint : STATE_DB_MAX_OBJECT_SIZE;

   vl_blob object_buffer;
   object_buffer.data.resize( get_args.buf_size );
   get_args.buf = object_buffer.data.data();

   statedb::get_object_result get_res;
   state->get_object( get_res, get_args );

   if( get_res.key == get_args.key )
      object_buffer.data.resize( get_res.size );
   else
      object_buffer.data.clear();
   return object_buffer;
}

SYSTEM_CALL_DEFINE( vl_blob, db_get_next_object, ((const statedb::object_space&) space, (const statedb::object_key&) key, (int32_t) object_size_hint) )
{
   auto state = context.get_state_node();
   KOINOS_ASSERT( state, database_exception, "Current state node does not exist", () );
   statedb::get_object_args get_args;
   get_args.space = space;
   get_args.key = key;
   get_args.buf_size = object_size_hint > 0 ? object_size_hint : STATE_DB_MAX_OBJECT_SIZE;

   vl_blob object_buffer;
   object_buffer.data.resize( get_args.buf_size );
   get_args.buf = object_buffer.data.data();

   statedb::get_object_result get_res;
   state->get_next_object( get_res, get_args );

   if( get_res.size > 0 )
      object_buffer.data.resize( get_res.size );
   else
      object_buffer.data.clear();

   return object_buffer;
}

SYSTEM_CALL_DEFINE( vl_blob, db_get_prev_object, ((const statedb::object_space&) space, (const statedb::object_key&) key, (int32_t) object_size_hint) )
{
   auto state = context.get_state_node();
   KOINOS_ASSERT( state, database_exception, "Current state node does not exist", () );
   statedb::get_object_args get_args;
   get_args.space = space;
   get_args.key = key;
   get_args.buf_size = object_size_hint > 0 ? object_size_hint : STATE_DB_MAX_OBJECT_SIZE;

   vl_blob object_buffer;
   object_buffer.data.resize( get_args.buf_size );
   get_args.buf = object_buffer.data.data();

   statedb::get_object_result get_res;
   state->get_prev_object( get_res, get_args );

   if( get_res.size > 0 )
      object_buffer.data.resize( get_res.size );
   else
      object_buffer.data.clear();

   return object_buffer;
}

} // koinos::chain
