
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
   error_data error;

   try
   {
      thunk_dispatcher::instance().call_thunk( tid, _ctx, ret_ptr, ret_len, arg_ptr, arg_len, bytes_written );
   }
   catch ( koinos::exception& e )
   {
      code = e.get_code();
      error = e.get_data();
   }

   if ( tid == system_call_id::exit )
   {
      if ( code >= chain::reversion )
         throw reversion_exception( code, error );
      if ( code <= chain::failure )
         throw failure_exception( code, error );

      throw success_exception( code );
   }

   if ( code != chain::success )
   {
      auto error_bytes = util::converter::as< std::string >( error );

      auto msg_len = error_bytes.size();
      if ( msg_len <= ret_len )
      {
         std::memcpy( ret_ptr, error_bytes.data(), msg_len );
         *bytes_written = uint32_t( msg_len );
      }
      else
      {
         code = chain::insufficient_return_buffer;
         *bytes_written = 0;
      }
   }

   return code;
}

int32_t host_api::invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len, uint32_t* bytes_written  )
{
   int32_t code = 0;
   error_data error;

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
               std::string args( arg_ptr, arg_len );
               auto res = _ctx.system_call( sid, args );
               code = res.code;

               if ( code )
                  error = res.res.error();
               else if ( res.res.has_object() )
               {
                  auto obj_len = res.res.object().size();
                  KOINOS_ASSERT( obj_len <= ret_len, insufficient_return_buffer_exception, "return buffer is not large enough for the return value" );
                  memcpy( ret_ptr, res.res.object().data(), obj_len );
                  *bytes_written = uint32_t( obj_len );
               }
               else
                  *bytes_written = 0;
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
               thunk_dispatcher::instance().call_thunk( thunk_id, _ctx, ret_ptr, ret_len, arg_ptr, arg_len, bytes_written );
            }
         }
      );
   }
   catch ( const koinos::exception& e )
   {
      error = e.get_data();
      code = e.get_code();
   }

   if ( _ctx.get_privilege() == privilege::user_mode && code >= reversion )
      throw reversion_exception( code, error );

   if ( sid == system_call_id::exit )
   {
      if ( code >= reversion )
         throw reversion_exception( code, error );
      if ( code <= failure )
         throw failure_exception( code, error );

      throw success_exception();
   }

   if ( code != chain::success )
   {
      auto error_bytes = util::converter::as< std::string >( error );

      auto msg_len = error_bytes.size();
      if ( msg_len <= ret_len )
      {
         std::memcpy( ret_ptr, error_bytes.data(), msg_len );
         *bytes_written = uint32_t( msg_len );
      }
      else
      {
         code = chain::insufficient_return_buffer;
         *bytes_written = 0;
      }
   }

   return code;
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
