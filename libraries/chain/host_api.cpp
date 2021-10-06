
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
   std::string blob_target;

   with_stack_frame(
      _ctx,
      stack_frame {
         .call = crypto::hash( crypto::multicodec::ripemd_160, std::string( "invoke_system_call" ) ).digest(),
         .call_privilege = privilege::kernel_mode,
      },
      [&]() {
         blob_target = thunk::get_object(
            _ctx,
            database::space::system_call_dispatch,
            key,
            database::system_call_dispatch::max_object_size
         ).value();
      }
   );

   protocol::system_call_target target;

   if ( blob_target.size() )
   {
      target.ParseFromString( blob_target );
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
      KOINOS_TODO( "Brainstorm how to avoid arg/ret copy and validate pointers" );
      std::string args( arg_ptr, arg_len );
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

int64_t host_api::get_meter_ticks() const
{
   auto compute_bandwidth_remaining = _ctx.resource_meter().compute_bandwidth_remaining();

   // If we have more ticks than fizzy can accept
   if ( compute_bandwidth_remaining > std::numeric_limits< int64_t >::max() )
      compute_bandwidth_remaining = std::numeric_limits< int64_t >::max();

   int64_t ticks = compute_bandwidth_remaining;
   return ticks;
}

void host_api::set_meter_ticks( int64_t meter_ticks )
{
   if ( meter_ticks < 0 )
   {
      _ctx.resource_meter().use_compute_bandwidth( _ctx.resource_meter().compute_bandwidth_remaining() );
      _ctx.resource_meter().use_compute_bandwidth( 1 );
   }
   else
   {
      // In the case where we've started out with more rc than possible ticks, we instead use max int64_t.
      // This should prevent a user with an extraordinary amount of rc from not being able to transact after
      // only using max int64_t worth of rc.
      auto compute_remaining = _ctx.resource_meter().compute_bandwidth_remaining();
      compute_remaining = compute_remaining > std::numeric_limits< int64_t >::max() ? std::numeric_limits< int64_t >::max() : compute_remaining;
      _ctx.resource_meter().use_compute_bandwidth( compute_remaining - meter_ticks );
   }
}

} // koinos::chain
