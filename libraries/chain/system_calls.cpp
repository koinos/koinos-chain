#include <koinos/chain/system_calls.hpp>

namespace koinos::chain {

// System Call Table API
//
// The system_call_table keeps track of system calls that have been overridden. It is
// called from the public system call slot automatically defined by the SYSTEM_CALL_DEFINE
// macro.

bool system_call_table::overridable( system_call_slot s ) noexcept
{
   return static_cast< uint32_t >( s ) % 2 == 0;
}

void system_call_table::update()
{
   for ( auto& [ key, value ] : pending_updates )
      system_call_map[ key ] = value;

   pending_updates.clear();
}

std::optional< system_call_bundle > system_call_table::get_system_call( system_call_slot s )
{
   if ( system_call_map.find( s ) != system_call_map.end() )
      return system_call_map[ s ];
   return {};
}

void system_call_table::set_system_call( system_call_slot s, system_call_bundle v )
{
   KOINOS_ASSERT( overridable( s ), system_call_not_overridable, "system call cannot be overridden" );
   pending_updates[ s ] = v;
}

// System API Definitions
//
// Using the SYSTEM_CALL_DEFINE macro will define the public facing system call for you. It will check the
// system_call_table class to see if an override exists and call it. If there is no system call override for
// the given system call it will call the native implement that you define.
//
// The native implementation should check whether or not we are in kernel mode and throw if we are not. This
// will prevent the user mode contract from circumventing the public interface.

system_api::system_api( apply_context& _ctx ) : context( _ctx ) {}

SYSTEM_CALL_DEFINE( void, abort )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   KOINOS_ASSERT( false, abort_called, "abort() called");
}

SYSTEM_CALL_DEFINE( void, eosio_assert, ((bool) condition, (null_terminated_ptr) msg) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   constexpr size_t max_assert_message = 1024;
   if( BOOST_UNLIKELY( !condition ) )
   {
      const size_t sz = strnlen( msg, max_assert_message );
      std::string message( msg, sz );
      KOINOS_THROW( chain_exception, "assertion failure with message: ${s}", ("s",message) );
   }
}

SYSTEM_CALL_DEFINE( void, eosio_assert_message, ((bool) condition, (array_ptr<const char>) msg, (uint32_t) len) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   constexpr size_t max_assert_message = 1024;
   if( BOOST_UNLIKELY( !condition ) )
   {
      const size_t sz = strnlen( msg, max_assert_message );
      std::string message( msg, sz );
      KOINOS_THROW( chain_exception, "assertion failure with message: ${s}", ("s",message) );
   }
}

SYSTEM_CALL_DEFINE( void, eosio_assert_code, ((bool) condition, (uint64_t) error_code) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
}
SYSTEM_CALL_DEFINE( void, eosio_exit, ((int32_t) code) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
}

SYSTEM_CALL_DEFINE( int, read_action_data, ((array_ptr<char>) memory, (uint32_t) buffer_size) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
//   auto s = ctx.get_action().data.size();
//   if( buffer_size == 0 ) return s;
//
//   auto copy_size = std::min( static_cast<size_t>(buffer_size), s );
//   memcpy( (char*)memory.value, context.get_action().data.data(), copy_size );
//
//   return copy_size;
   return 0;
}

SYSTEM_CALL_DEFINE( int, action_data_size )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   return 0;
}

SYSTEM_CALL_DEFINE( name, current_receiver )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   return context.receiver;
}

SYSTEM_CALL_DEFINE( char*, memcpy, ((array_ptr< char >) dest, (array_ptr< const char >) src, (uint32_t) length) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   KOINOS_ASSERT((size_t)(std::abs((ptrdiff_t)dest.value - (ptrdiff_t)src.value)) >= length, chain_exception, "memcpy can only accept non-aliasing pointers");
   return (char *)::memcpy(dest, src, length);
}

SYSTEM_CALL_DEFINE( char*, memmove, ((array_ptr< char >) dest, (array_ptr< const char >) src, (uint32_t) length) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   return (char *)::memmove(dest, src, length);
}

SYSTEM_CALL_DEFINE( int, memcmp, ((array_ptr< const char >) dest, (array_ptr< const char >) src, (uint32_t) length) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   int ret = ::memcmp(dest, src, length);
   if(ret < 0)
      return -1;
   if(ret > 0)
      return 1;
   return 0;
}

SYSTEM_CALL_DEFINE( char*, memset, ((array_ptr< char >) dest, (int) value, (uint32_t) length) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   return (char *)::memset( dest, value, length );
}

SYSTEM_CALL_DEFINE( void, prints, ((null_terminated_ptr) str) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   context.console_append( static_cast< const char* >(str) );
}

SYSTEM_CALL_DEFINE( void, prints_l, ((array_ptr< const char >) str, (uint32_t) str_len ) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   context.console_append(string(str, str_len));
}

SYSTEM_CALL_DEFINE( void, printi, ((int64_t) val) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   std::ostringstream oss;
   oss << val;
   context.console_append( oss.str() );
}

SYSTEM_CALL_DEFINE( void, printui, ((uint64_t) val) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   std::ostringstream oss;
   oss << val;
   context.console_append( oss.str() );
}

SYSTEM_CALL_DEFINE( void, printi128, ((const __int128&) val) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   bool is_negative = (val < 0);
   unsigned __int128 val_magnitude;

   if( is_negative )
      val_magnitude = static_cast<unsigned __int128>(-val); // Works even if val is at the lowest possible value of a int128_t
   else
      val_magnitude = static_cast<unsigned __int128>(val);

   fc::uint128_t v(val_magnitude>>64, static_cast<uint64_t>(val_magnitude) );

   string s;
   if( is_negative ) {
      s += '-';
   }
   s += fc::variant(v).get_string();

   context.console_append( s );
}

SYSTEM_CALL_DEFINE( void, printui128, ((const unsigned __int128&) val) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   fc::uint128_t v(val>>64, static_cast<uint64_t>(val) );
   context.console_append(fc::variant(v).get_string());
}

SYSTEM_CALL_DEFINE( void, printsf, ((float) val) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   // Assumes float representation on native side is the same as on the WASM side
   std::ostringstream oss;
   oss.setf( std::ios::scientific, std::ios::floatfield );
   oss.precision( std::numeric_limits<float>::digits10 );
   oss << val;
   context.console_append( oss.str() );
}

SYSTEM_CALL_DEFINE( void, printdf, ((double) val) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   // Assumes double representation on native side is the same as on the WASM side
   std::ostringstream oss;
   oss.setf( std::ios::scientific, std::ios::floatfield );
   oss.precision( std::numeric_limits<double>::digits10 );
   oss << val;
   context.console_append( oss.str() );
}

SYSTEM_CALL_DEFINE( void, printqf, ((const float128_t&) val) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   /*
    * Native-side long double uses an 80-bit extended-precision floating-point number.
    * The easiest solution for now was to use the Berkeley softfloat library to round the 128-bit
    * quadruple-precision floating-point number to an 80-bit extended-precision floating-point number
    * (losing precision) which then allows us to simply cast it into a long double for printing purposes.
    *
    * Later we might find a better solution to print the full quadruple-precision floating-point number.
    * Maybe with some compilation flag that turns long double into a quadruple-precision floating-point number,
    * or maybe with some library that allows us to print out quadruple-precision floating-point numbers without
    * having to deal with long doubles at all.
    */

   std::ostringstream oss;
   oss.setf( std::ios::scientific, std::ios::floatfield );

#ifdef __x86_64__
   oss.precision( std::numeric_limits<long double>::digits10 );
   extFloat80_t val_approx;
   f128M_to_extF80M(&val, &val_approx);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
   oss << *(long double*)(&val_approx);
#pragma GCC diagnostic pop
#else
   oss.precision( std::numeric_limits<double>::digits10 );
   double val_approx = from_softfloat64( f128M_to_f64(&val) );
   oss << val_approx;
#endif
   context.console_append( oss.str() );
}

SYSTEM_CALL_DEFINE( void, printn, ((name) value) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   context.console_append(value.to_string());
}

SYSTEM_CALL_DEFINE( void, printhex, ((array_ptr< const char >) data, (uint32_t) data_len ) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   context.console_append(fc::to_hex(data, data_len));
}

SYSTEM_CALL_DEFINE( int, db_store_i64, ((uint64_t) scope, (uint64_t) table, (uint64_t) payer, (uint64_t) id, (array_ptr<const char>) buffer, (uint32_t) buffer_size) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   return context.db_store_i64( name(scope), name(table), account_name(payer), id, buffer, buffer_size );
}

SYSTEM_CALL_DEFINE( void, db_update_i64, ((int) itr, (uint64_t) payer, (array_ptr< const char >) buffer, (uint32_t) buffer_size) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   context.db_update_i64( itr, account_name(payer), buffer, buffer_size );
}

SYSTEM_CALL_DEFINE( void, db_remove_i64, ((int) itr) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   context.db_remove_i64( itr );
}

SYSTEM_CALL_DEFINE( int, db_get_i64, ((int) itr, (array_ptr< char >) buffer, (uint32_t) buffer_size) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   return context.db_get_i64( itr, buffer, buffer_size );
}

SYSTEM_CALL_DEFINE( int, db_next_i64, ((int) itr, (uint64_t&) primary) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   return context.db_next_i64(itr, primary);
}

SYSTEM_CALL_DEFINE( int, db_previous_i64, ((int) itr, (uint64_t&) primary) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   return context.db_previous_i64(itr, primary);
}

SYSTEM_CALL_DEFINE( int, db_find_i64, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (uint64_t) id) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   return context.db_find_i64( name(code), name(scope), name(table), id );
}

SYSTEM_CALL_DEFINE( int, db_lowerbound_i64, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (uint64_t) id) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   return context.db_lowerbound_i64( name(code), name(scope), name(table), id );
}

SYSTEM_CALL_DEFINE( int, db_upperbound_i64, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (uint64_t) id) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   return context.db_upperbound_i64( name(code), name(scope), name(table), id );
}

SYSTEM_CALL_DEFINE( int, db_end_i64, ((uint64_t) code, (uint64_t) scope, (uint64_t) table) )
{
   KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );
   return context.db_end_i64( name(code), name(scope), name(table) );
}

SYSTEM_CALL_DB_WRAPPERS_SIMPLE_SECONDARY(idx64,  uint64_t)
SYSTEM_CALL_DB_WRAPPERS_SIMPLE_SECONDARY(idx128, uint128_t)
SYSTEM_CALL_DB_WRAPPERS_ARRAY_SECONDARY(idx256, 2, uint128_t)
SYSTEM_CALL_DB_WRAPPERS_FLOAT_SECONDARY(idx_double, float64_t)
SYSTEM_CALL_DB_WRAPPERS_FLOAT_SECONDARY(idx_long_double, float128_t)


} // koinos::chain
