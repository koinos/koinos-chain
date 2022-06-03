
#include <koinos/chain/constants.hpp>
#include <koinos/chain/host_api.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/system_call_ids.pb.h>

#include <koinos/log.hpp>
#include <koinos/util/conversion.hpp>

using namespace std::string_literals;

namespace koinos::chain {

host_api::host_api( execution_context& ctx ) : _ctx( ctx ) {}
host_api::~host_api() {}

int32_t host_api::invoke_thunk( uint32_t tid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len, uint32_t* bytes_written  )
{
   KOINOS_ASSERT( _ctx.get_privilege() == privilege::kernel_mode, insufficient_privileges_exception, "'invoke_thunk' must be called from a system context" );

   int32_t code = 0;

   try
   {
      code = thunk_dispatcher::instance().call_thunk( tid, _ctx, ret_ptr, ret_len, arg_ptr, arg_len, bytes_written );
   }
   catch ( koinos::exception& e )
   {
      if ( e.get_code() != chain::success )
      {
         code = e.get_code();
         auto msg_len = e.get_message().size() + 1;
         if ( msg_len <= ret_len )
         {
            std::memcpy( ret_ptr, e.get_message().c_str(), msg_len );
            *bytes_written = msg_len;
         }
         else
         {
            code = chain::insufficient_return_buffer;
            *bytes_written = 0;
         }
      }
   }

   if ( tid == system_call_id::exit )
   {
      if ( code >= chain::reversion )
         throw reversion_exception( code, "" );
      if ( code <= chain::failure )
         throw failure_exception( code, "" );

      KOINOS_THROW( success_exception, "" );
   }

   return code;
}

int32_t host_api::invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len, uint32_t* bytes_written  )
{
   int32_t retcode = 0;
   std::pair< std::string, int32_t > res;

   try
   {
      auto key = util::converter::as< std::string >( sid );
      database_object object;

      with_stack_frame(
         _ctx,
         stack_frame {
            .sid = sid,
            .call_privilege = privilege::kernel_mode
         },
         [&]() {
            if ( _ctx.system_call_exists( sid ) )
            {
               #pragma message "TODO: Brainstorm how to avoid arg/ret copy and validate pointers"
               std::string args( arg_ptr, arg_len );
               res = _ctx.system_call( sid, args );
            }
            else
            {
               auto thunk_id = _ctx.thunk_translation( sid );
               KOINOS_ASSERT( thunk_dispatcher::instance().thunk_exists( thunk_id ), unknown_thunk_exception, "thunk ${tid} does not exist", ("tid", thunk_id) );
               auto desc = chain::system_call_id_descriptor();
               auto enum_value = desc->FindValueByNumber( thunk_id );
               KOINOS_ASSERT( enum_value, unknown_thunk_exception, "unrecognized thunk id ${id}", ("id", thunk_id) );
               auto compute = _ctx.get_compute_bandwidth( enum_value->name() );
               _ctx.resource_meter().use_compute_bandwidth( compute );
               retcode = thunk_dispatcher::instance().call_thunk( thunk_id, _ctx, ret_ptr, ret_len, arg_ptr, arg_len, bytes_written );
            }
         }
      );
   }
   catch ( const koinos::exception& e )
   {
      res.first = e.get_message();
      res.second = e.get_code();
   }

   if ( res.second != chain::success )
   {
      auto msg_len = res.first.size() + 1;
      if ( msg_len <= ret_len )
      {
         std::memcpy( ret_ptr, res.first.c_str(), msg_len );
         *bytes_written = msg_len;
      }
      else
      {
         res.second = chain::insufficient_return_buffer;
         *bytes_written = 0;
      }
   }

   if ( _ctx.get_privilege() == privilege::user_mode && res.second >= reversion )
      throw reversion_exception( res.second, res.first );

   if ( sid == system_call_id::exit )
   {
      if ( res.second >= reversion )
         throw reversion_exception( res.second, res.first );
      if ( res.second <= failure )
         throw failure_exception( res.second, res.first );

      KOINOS_THROW( success_exception, res.first );
   }

   return res.second;
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

void host_api::use_meter_ticks( uint64_t meter_ticks )
{
   if ( meter_ticks > _ctx.resource_meter().compute_bandwidth_remaining() )
   {
      _ctx.resource_meter().use_compute_bandwidth( _ctx.resource_meter().compute_bandwidth_remaining() );
      _ctx.resource_meter().use_compute_bandwidth( 1 );
   }
   else
   {
      _ctx.resource_meter().use_compute_bandwidth( meter_ticks );
   }
}

} // koinos::chain
