#include <koinos/pack/rt/pack_fwd.hpp>

#include <koinos/chain/host.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/system_calls.hpp>

#include <koinos/pack/rt/string.hpp>

#include <koinos/log.hpp>
#include <koinos/util.hpp>

namespace koinos::chain {

host_api::host_api( apply_context& _ctx ) : context( _ctx ) {}

void host_api::invoke_thunk( uint32_t tid, array_ptr< char > ret_ptr, uint32_t ret_len, array_ptr< const char > arg_ptr, uint32_t arg_len )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, "cannot be called directly from user mode" );
   thunk_dispatcher::instance().call_thunk( thunk_id(tid), context, ret_ptr, ret_len, arg_ptr, arg_len );
}

void host_api::invoke_system_call( uint32_t sid, array_ptr< char > ret_ptr, uint32_t ret_len, array_ptr< const char > arg_ptr, uint32_t arg_len )
{
   using protocol::thunk_id_type;
   using protocol::contract_id_type;

   // TODO Do we need to invoke serialization here?
   statedb::object_key key = sid;

   vl_blob vl_target =
      thunk::db_get_object_thunk( context, SYS_CALL_DISPATCH_TABLE_SPACE_ID, key, SYS_CALL_DISPATCH_TABLE_OBJECT_MAX_SIZE );

   if( vl_target.data.size() == 0 )
   {
      vl_target = get_default_sys_call_entry( sid );
      KOINOS_ASSERT( vl_target.data.size() > 0,
         unknown_system_call,
         "system call table dispatch entry ${sid} does not exist",
         ("sid", sid)
         );
   }

   protocol::sys_call_target target;

   koinos::pack::from_vl_blob( vl_target, target );

   std::visit(
      koinos::overloaded{
         [&]( thunk_id_type& tid ) {
            thunk_dispatcher::instance().call_thunk( tid, context, ret_ptr, ret_len, arg_ptr, arg_len );
         },
         [&]( contract_id_type& cid ) {
#pragma message( "TODO:  Invoke smart contract sys call handler" )
         },
         [&]( auto& ) {
            KOINOS_THROW( unknown_system_call, "system call table dispatch entry ${sid} has unimplemented type ${tag}",
               ("sid", sid)("tag", target.index()) );
         } }, target );
}

} // koinos::chain
