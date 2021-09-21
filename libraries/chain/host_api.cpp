
#include <koinos/chain/constants.hpp>
#include <koinos/chain/host_api.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/system_calls.hpp>

#include <koinos/conversion.hpp>
#include <koinos/log.hpp>
#include <koinos/util.hpp>

namespace koinos::chain {

host_api::host_api( apply_context& ctx ) : _ctx( ctx ) {}
host_api::~host_api() {}

void host_api::invoke_thunk( uint32_t tid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len )
{
   KOINOS_ASSERT( _ctx.get_privilege() == privilege::kernel_mode, insufficient_privileges, "cannot be called directly from user mode" );
   thunk_dispatcher::instance().call_thunk( tid, _ctx, ret_ptr, ret_len, arg_ptr, arg_len );
}

void host_api::invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len )
{
   auto key = converter::as< std::string >( sid );
   std::string blob_bundle;

   with_stack_frame(
      _ctx,
      stack_frame {
         .call = crypto::hash( crypto::multicodec::ripemd_160, std::string( "invoke_system_call" ) ).digest(),
         .call_privilege = privilege::kernel_mode,
      },
      [&]() {
         blob_bundle = thunk::db_get_object(
            _ctx,
            database::space::system_call_dispatch,
            key,
            database::system_call_dispatch::max_object_size
         ).value();
      }
   );

   protocol::system_call_target target;

   if ( blob_bundle.size() )
   {
      auto bundle = target.mutable_system_call_bundle();
      bundle->ParseFromString( blob_bundle );
   }
   else
   {
      target.set_thunk_id( sid );
   }

   if ( target.thunk_id() )
   {
      with_stack_frame(
         _ctx,
         stack_frame {
            .call = crypto::hash( crypto::multicodec::ripemd_160, std::string( "invoke_system_call" ) ).digest(),
            .call_privilege = _ctx.get_privilege(),
         },
         [&]() {
            thunk_dispatcher::instance().call_thunk( target.thunk_id(), _ctx, ret_ptr, ret_len, arg_ptr, arg_len );
         }
      );
   }
   else if ( target.has_system_call_bundle() )
   {
      const auto& scb = target.system_call_bundle();
      std::string args;
      KOINOS_TODO( "Brainstorm how to avoid arg/ret copy and validate pointers" );
      args.resize( arg_len );
      std::memcpy( args.data(), arg_ptr, arg_len );
      std::string ret;
      with_stack_frame(
         _ctx,
         stack_frame {
            .call = crypto::hash( crypto::multicodec::ripemd_160, std::string( "invoke_system_call" ) ).digest(),
            .call_privilege = privilege::kernel_mode,
         },
         [&]()
         {
            ret = thunk::call_contract( _ctx, scb.contract_id(), scb.entry_point(), args ).value();
         }
      );
      KOINOS_ASSERT( ret.size() <= ret_len, insufficient_return_buffer, "return buffer too small" );
      std::memcpy( ret.data(), ret_ptr, ret.size() );
   }
   else
   {
      KOINOS_THROW( thunk_not_found, "did not find system call or thunk with id: ${id}", ("id", sid) );
   }
}

} // koinos::chain
