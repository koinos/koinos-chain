#include <koinos/pack/rt/pack_fwd.hpp>

#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/xcalls.hpp>

#include <koinos/pack/rt/string.hpp>

#include <koinos/log.hpp>
#include <koinos/util.hpp>

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
      thunk::db_get_object_thunk( context, XCALL_DISPATCH_TABLE_SPACE_ID, key, XCALL_DISPATCH_TABLE_OBJECT_MAX_SIZE );

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

} // koinos::chain
