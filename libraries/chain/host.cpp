#include <koinos/pack/rt/pack_fwd.hpp>

#include <koinos/chain/constants.hpp>
#include <koinos/chain/host.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/system_calls.hpp>

#include <koinos/log.hpp>
#include <koinos/util.hpp>

namespace koinos::chain {

host_api::host_api( apply_context& _ctx ) : context( _ctx ) {}

void host_api::invoke_thunk( thunk_id_type tid, array_ptr< char > ret_ptr, uint32_t ret_len, array_ptr< const char > arg_ptr, uint32_t arg_len )
{
   KOINOS_ASSERT( context.get_privilege() == privilege::kernel_mode, insufficient_privileges, "cannot be called directly from user mode" );
   thunk_dispatcher::instance().call_thunk( thunk_id( tid ), context, ret_ptr, ret_len, arg_ptr, arg_len );
}

void host_api::invoke_system_call( system_call_id_type sid, array_ptr< char > ret_ptr, uint32_t ret_len, array_ptr< const char > arg_ptr, uint32_t arg_len )
{
   // TODO Do we need to invoke serialization here?
   statedb::object_key key = sid;
   variable_blob blob_target;

   with_stack_frame(
      context,
      stack_frame {
         .call = crypto::hash( CRYPTO_RIPEMD160_ID, std::string( "invoke_system_call" ) ).digest,
         .call_privilege = privilege::kernel_mode,
      },
      [&]() {
         blob_target = thunk::db_get_object(
            context,
            database::system_call_dispatch_space,
            key,
            database::system_call_dispatch_object_max_size
         );
      }
   );

   system_call_target target;

   if( blob_target.size() == 0 )
   {
      auto maybe_thunk_id = get_default_system_call_entry( system_call_id( sid ) );
      KOINOS_ASSERT( maybe_thunk_id,
         unknown_system_call,
         "system call table dispatch entry ${sid} does not exist",
         ("sid", sid)
      );
      target = *maybe_thunk_id;
   }
   else
   {
      koinos::pack::from_variable_blob( blob_target, target );
   }

   std::visit(
      koinos::overloaded{
         [&]( thunk_id& tid ) {
            with_stack_frame(
               context,
               stack_frame {
                  .call = crypto::hash( CRYPTO_RIPEMD160_ID, std::string( "invoke_system_call" ) ).digest,
                  .call_privilege = context.get_privilege(),
               },
               [&]() {
                  thunk_dispatcher::instance().call_thunk( tid, context, ret_ptr, ret_len, arg_ptr, arg_len );
               }
            );
         },
         [&]( contract_call_bundle& scb ) {
            variable_blob args;
            KOINOS_TODO( "Brainstorm how to avoid arg/ret copy and validate pointers" )
            args.resize( arg_len );
            std::memcpy( args.data(), arg_ptr.value, arg_len );
            variable_blob ret;
            with_stack_frame(
               context,
               stack_frame {
                  .call = crypto::hash( CRYPTO_RIPEMD160_ID, std::string( "invoke_system_call" ) ).digest,
                  .call_privilege = privilege::kernel_mode,
               },
               [&]()
               {
                  ret = thunk::execute_contract( context, scb.contract_id, scb.entry_point, args );
               }
            );
            KOINOS_ASSERT( ret.size() <= ret_len, insufficient_return_buffer, "Return buffer too small" );
            std::memcpy( ret.data(), ret_ptr.value, ret.size() );
         },
         [&]( auto& ) {
            KOINOS_THROW( unknown_system_call, "system call table dispatch entry ${sid} has unimplemented type ${tag}",
               ("sid", sid)("tag", target.index()) );
         } }, target );
}

} // koinos::chain
