#include <koinos/chain/syscalls.hpp>
#include <koinos/chain/privilege.hpp>
#include <koinos/chain/apply_context.hpp>

#include <boost/preprocessor.hpp>

namespace koinos::chain {

bool syscall_table::overridable( syscall_slot s ) noexcept
{
   return static_cast< uint32_t >( s ) % 2 == 0;
}

void syscall_table::update()
{
   for ( auto& [ key, value ] : pending_updates )
      syscall_map[ key ] = value;

   pending_updates.clear();
}

std::optional< syscall_bundle > syscall_table::get_syscall( syscall_slot s )
{
   if ( syscall_map.find( s ) != syscall_map.end() )
      return syscall_map[ s ];
   return {};
}

void syscall_table::set_syscall( syscall_slot s, syscall_bundle v )
{
   KOINOS_ASSERT( overridable( s ), syscall_not_overridable, "syscall cannot be overridden" );
   pending_updates[ s ] = v;
}

// System API Definitions

system_api::system_api( apply_context& _ctx ) : context( _ctx ) {}

#define RET_TYPE_void 1)(1
#define IS_VOID(type) BOOST_PP_EQUAL(BOOST_PP_SEQ_SIZE((BOOST_PP_CAT(RET_TYPE_,type))),2)

#define REM(...) __VA_ARGS__
#define EAT(...)

// Retrieve the type
#define TYPEOF(x) DETAIL_TYPEOF(DETAIL_TYPEOF_PROBE x,)
#define DETAIL_TYPEOF(...) DETAIL_TYPEOF_HEAD(__VA_ARGS__)
#define DETAIL_TYPEOF_HEAD(x, ...) REM x
#define DETAIL_TYPEOF_PROBE(...) (__VA_ARGS__),
// Strip off the type
#define STRIP(x) EAT x
// Show the type without parenthesis
#define PAIR(x) REM x

#define DETAIL_DEFINE_MEMBERS_EACH(r, data, x) PAIR(x);
#define DETAIL_DEFINE_ARGS_EACH(r, data, i, x) BOOST_PP_COMMA_IF(i) PAIR(x)
#define DETAIL_DEFINE_FORWARD_EACH(r, data, i, x) BOOST_PP_COMMA_IF(i) STRIP(x)

#define DETAIL_DEFINE_MEMBERS(args) BOOST_PP_SEQ_FOR_EACH(DETAIL_DEFINE_MEMBERS_EACH, data, BOOST_PP_VARIADIC_TO_SEQ args)
#define DETAIL_DEFINE_ARGS(args) BOOST_PP_SEQ_FOR_EACH_I(DETAIL_DEFINE_ARGS_EACH, data, BOOST_PP_VARIADIC_TO_SEQ args)
#define DETAIL_DEFINE_FORWARD(args) BOOST_PP_SEQ_FOR_EACH_I(DETAIL_DEFINE_FORWARD_EACH, data, BOOST_PP_VARIADIC_TO_SEQ args)

#define SYSCALL_INTERNAL_CALL(x, ...) BOOST_PP_CAT(_,x)(DETAIL_DEFINE_FORWARD(__VA_ARGS__));
#define SYSCALL_INTERNAL_CALL_WITH_RETURN(ret, x, ...) ret = SYSCALL_INTERNAL_CALL(x, __VA_ARGS__);

#pragma message( "TODO: Use the syscall_bundle to actually call VM with arguments and retrieve return value" )
#define SYSCALL_DEFINE( RETURN_TYPE, SYSCALL, ... )                                   \
   RETURN_TYPE system_api::SYSCALL( DETAIL_DEFINE_ARGS(__VA_ARGS__) )                 \
   {                                                                                  \
      BOOST_PP_IF(IS_VOID(RETURN_TYPE),,RETURN_TYPE retval;)                          \
      auto current_level = context.privilege_level;                                   \
      context.privilege_level = privilege::kernel_mode;                               \
      try                                                                             \
      {                                                                               \
         auto call_override = context.syscalls.get_syscall( syscall_slot::SYSCALL );  \
         if ( call_override )                                                         \
         {                                                                            \
         }                                                                            \
         else                                                                         \
         {                                                                            \
            BOOST_PP_IF(                                                              \
               IS_VOID(RETURN_TYPE),                                                  \
               SYSCALL_INTERNAL_CALL(SYSCALL, __VA_ARGS__),                           \
               SYSCALL_INTERNAL_CALL_WITH_RETURN(retval, SYSCALL, __VA_ARGS__)        \
            )                                                                         \
         }                                                                            \
      }                                                                               \
      catch ( ... )                                                                   \
      {                                                                               \
         context.privilege_level = current_level;                                     \
         throw;                                                                       \
      }                                                                               \
      context.privilege_level = current_level;                                        \
      BOOST_PP_IF(IS_VOID(RETURN_TYPE),,return retval;)                               \
   }                                                                                  \
   RETURN_TYPE system_api::BOOST_PP_CAT(_,SYSCALL)( DETAIL_DEFINE_ARGS(__VA_ARGS__) )

#define DB_API_METHOD_WRAPPERS_SIMPLE_SECONDARY(IDX, TYPE)\
   SYSCALL_DEFINE( int, db_##IDX##_store, ((uint64_t) scope, (uint64_t) table, (uint64_t) payer, (uint64_t) id, (const TYPE&) secondary) ) {\
      return context.IDX.store( scope, table, account_name(payer), id, secondary );\
   }\
   SYSCALL_DEFINE( void, db_##IDX##_update, ((int) iterator, (uint64_t) payer, (const TYPE&) secondary) ) {\
      return context.IDX.update( iterator, account_name(payer), secondary );\
   }\
   SYSCALL_DEFINE( void, db_##IDX##_remove, ((int) iterator ) ) {\
      return context.IDX.remove( iterator );\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_find_secondary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (const TYPE&) secondary, (uint64_t&) primary) ) {\
      return context.IDX.find_secondary(code, scope, table, secondary, primary);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_find_primary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (TYPE&) secondary, (uint64_t) primary) ) {\
      return context.IDX.find_primary(code, scope, table, secondary, primary);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_lowerbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (TYPE&) secondary, (uint64_t&) primary) ) {\
      return context.IDX.lowerbound_secondary(code, scope, table, secondary, primary);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_upperbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (TYPE&) secondary, (uint64_t&) primary) ) {\
      return context.IDX.upperbound_secondary(code, scope, table, secondary, primary);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_end, ((uint64_t) code, (uint64_t) scope, (uint64_t) table) ) {\
      return context.IDX.end_secondary(code, scope, table);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_next, ((int) iterator, (uint64_t&) primary)  ) {\
      return context.IDX.next_secondary(iterator, primary);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_previous, ((int) iterator, (uint64_t&) primary) ) {\
      return context.IDX.previous_secondary(iterator, primary);\
   }

#define DB_API_METHOD_WRAPPERS_ARRAY_SECONDARY(IDX, ARR_SIZE, ARR_ELEMENT_TYPE)\
   SYSCALL_DEFINE( int, db_##IDX##_store, ((uint64_t) scope, (uint64_t) table, (uint64_t) payer, (uint64_t) id, (array_ptr<const ARR_ELEMENT_TYPE>) data, (uint32_t) data_len) ) {\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
         return context.IDX.store(scope, table, account_name(payer), id, data.value);\
   }\
   SYSCALL_DEFINE( void, db_##IDX##_update, ((int) iterator, (uint64_t) payer, (array_ptr<const ARR_ELEMENT_TYPE>) data, (uint32_t) data_len) ) {\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
      return context.IDX.update(iterator, account_name(payer), data.value);\
   }\
   SYSCALL_DEFINE( void, db_##IDX##_remove, ((int) iterator ) ) {\
      return context.IDX.remove( iterator );\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_find_secondary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (array_ptr<const ARR_ELEMENT_TYPE>) data, (uint32_t) data_len, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
      return context.IDX.find_secondary(code, scope, table, data, primary);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_find_primary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (array_ptr<ARR_ELEMENT_TYPE>) data, (uint32_t) data_len, (uint64_t) primary) ) {\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
      return context.IDX.find_primary(code, scope, table, data.value, primary);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_lowerbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (array_ptr<ARR_ELEMENT_TYPE>) data, (uint32_t) data_len, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
      return context.IDX.lowerbound_secondary(code, scope, table, data.value, primary);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_upperbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (array_ptr<ARR_ELEMENT_TYPE>) data, (uint32_t) data_len, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
      return context.IDX.upperbound_secondary(code, scope, table, data.value, primary);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_end, ((uint64_t) code, (uint64_t) scope, (uint64_t) table) ) {\
      return context.IDX.end_secondary(code, scope, table);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_next, ((int) iterator, (uint64_t&) primary)  ) {\
      return context.IDX.next_secondary(iterator, primary);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_previous, ((int) iterator, (uint64_t&) primary) ) {\
      return context.IDX.previous_secondary(iterator, primary);\
   }

#define DB_API_METHOD_WRAPPERS_FLOAT_SECONDARY(IDX, TYPE)\
   SYSCALL_DEFINE( int, db_##IDX##_store, ((uint64_t) scope, (uint64_t) table, (uint64_t) payer, (uint64_t) id, (const TYPE&) secondary) ) {\
      KOINOS_ASSERT( !softfloat_api::is_nan( secondary ), transaction_exception, "NaN is not an allowed value for a secondary key" );\
      return context.IDX.store( scope, table, account_name(payer), id, secondary );\
   }\
   SYSCALL_DEFINE( void, db_##IDX##_update, ((int) iterator, (uint64_t) payer, (const TYPE&) secondary) ) {\
      KOINOS_ASSERT( !softfloat_api::is_nan( secondary ), transaction_exception, "NaN is not an allowed value for a secondary key" );\
      return context.IDX.update( iterator, account_name(payer), secondary );\
   }\
   SYSCALL_DEFINE( void, db_##IDX##_remove, ((int) iterator ) ) {\
      return context.IDX.remove( iterator );\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_find_secondary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (const TYPE&) secondary, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( !softfloat_api::is_nan( secondary ), transaction_exception, "NaN is not an allowed value for a secondary key" );\
      return context.IDX.find_secondary(code, scope, table, secondary, primary);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_find_primary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (TYPE&) secondary, (uint64_t) primary) ) {\
      return context.IDX.find_primary(code, scope, table, secondary, primary);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_lowerbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (TYPE&) secondary, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( !softfloat_api::is_nan( secondary ), transaction_exception, "NaN is not an allowed value for a secondary key" );\
      return context.IDX.lowerbound_secondary(code, scope, table, secondary, primary);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_upperbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (TYPE&) secondary, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( !softfloat_api::is_nan( secondary ), transaction_exception, "NaN is not an allowed value for a secondary key" );\
      return context.IDX.upperbound_secondary(code, scope, table, secondary, primary);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_end, ((uint64_t) code, (uint64_t) scope, (uint64_t) table) ) {\
      return context.IDX.end_secondary(code, scope, table);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_next, ((int) iterator, (uint64_t&) primary)  ) {\
      return context.IDX.next_secondary(iterator, primary);\
   }\
   SYSCALL_DEFINE( int, db_##IDX##_previous, ((int) iterator, (uint64_t&) primary) ) {\
      return context.IDX.previous_secondary(iterator, primary);\
   }

SYSCALL_DEFINE( void, abort )
{
   KOINOS_ASSERT( false, abort_called, "abort() called");
}

SYSCALL_DEFINE( void, eosio_assert, ((bool) condition, (null_terminated_ptr) msg) )
{
   static constexpr size_t max_assert_message = 1024;
   if( BOOST_UNLIKELY( !condition ) )
   {
      const size_t sz = strnlen( msg, max_assert_message );
      std::string message( msg, sz );
      KOINOS_THROW( chain_exception, "assertion failure with message: ${s}", ("s",message) );
   }
}

SYSCALL_DEFINE( void, eosio_assert_message, ((bool) condition, (array_ptr<const char>) msg, (uint32_t) len) )
{
   static constexpr size_t max_assert_message = 1024;
   if( BOOST_UNLIKELY( !condition ) )
   {
      const size_t sz = strnlen( msg, max_assert_message );
      std::string message( msg, sz );
      KOINOS_THROW( chain_exception, "assertion failure with message: ${s}", ("s",message) );
   }
}

SYSCALL_DEFINE( void, eosio_assert_code, ((bool) condition, (uint64_t) error_code) ) {}
SYSCALL_DEFINE( void, eosio_exit, ((int32_t) code) ) {}

SYSCALL_DEFINE( int, read_action_data, ((array_ptr<char>) memory, (uint32_t) buffer_size) )
{
//   auto s = ctx.get_action().data.size();
//   if( buffer_size == 0 ) return s;
//
//   auto copy_size = std::min( static_cast<size_t>(buffer_size), s );
//   memcpy( (char*)memory.value, context.get_action().data.data(), copy_size );
//
//   return copy_size;
   return 0;
}

SYSCALL_DEFINE( int, action_data_size )
{
   return 0;
}

SYSCALL_DEFINE( name, current_receiver )
{
   return context.receiver;
}

SYSCALL_DEFINE( char*, memcpy, ((array_ptr< char >) dest, (array_ptr< const char >) src, (uint32_t) length) )
{
   KOINOS_ASSERT((size_t)(std::abs((ptrdiff_t)dest.value - (ptrdiff_t)src.value)) >= length, chain_exception, "memcpy can only accept non-aliasing pointers");
   return (char *)::memcpy(dest, src, length);
}

SYSCALL_DEFINE( char*, memmove, ((array_ptr< char >) dest, (array_ptr< const char >) src, (uint32_t) length) )
{
   return (char *)::memmove(dest, src, length);
}

SYSCALL_DEFINE( int, memcmp, ((array_ptr< const char >) dest, (array_ptr< const char >) src, (uint32_t) length) )
{
   int ret = ::memcmp(dest, src, length);
   if(ret < 0)
      return -1;
   if(ret > 0)
      return 1;
   return 0;
}

SYSCALL_DEFINE( char*, memset, ((array_ptr< char >) dest, (int) value, (uint32_t) length) )
{
   return (char *)::memset( dest, value, length );
}

SYSCALL_DEFINE( void, prints, ((null_terminated_ptr) str) )
{
   context.console_append( static_cast< const char* >(str) );
}

SYSCALL_DEFINE( void, prints_l, ((array_ptr< const char >) str, (uint32_t) str_len ) )
{
   context.console_append(string(str, str_len));
}

SYSCALL_DEFINE( void, printi, ((int64_t) val) )
{
   std::ostringstream oss;
   oss << val;
   context.console_append( oss.str() );
}

SYSCALL_DEFINE( void, printui, ((uint64_t) val) )
{
   std::ostringstream oss;
   oss << val;
   context.console_append( oss.str() );
}

SYSCALL_DEFINE( void, printi128, ((const __int128&) val) )
{
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

SYSCALL_DEFINE( void, printui128, ((const unsigned __int128&) val) )
{
   fc::uint128_t v(val>>64, static_cast<uint64_t>(val) );
   context.console_append(fc::variant(v).get_string());
}

SYSCALL_DEFINE( void, printsf, ((float) val) )
{
   // Assumes float representation on native side is the same as on the WASM side
   std::ostringstream oss;
   oss.setf( std::ios::scientific, std::ios::floatfield );
   oss.precision( std::numeric_limits<float>::digits10 );
   oss << val;
   context.console_append( oss.str() );
}

SYSCALL_DEFINE( void, printdf, ((double) val) )
{
   // Assumes double representation on native side is the same as on the WASM side
   std::ostringstream oss;
   oss.setf( std::ios::scientific, std::ios::floatfield );
   oss.precision( std::numeric_limits<double>::digits10 );
   oss << val;
   context.console_append( oss.str() );
}

SYSCALL_DEFINE( void, printqf, ((const float128_t&) val) )
{
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

SYSCALL_DEFINE( void, printn, ((name) value) )
{
   context.console_append(value.to_string());
}

SYSCALL_DEFINE( void, printhex, ((array_ptr< const char >) data, (uint32_t) data_len ) )
{
   context.console_append(fc::to_hex(data, data_len));
}

SYSCALL_DEFINE( int, db_store_i64, ((uint64_t) scope, (uint64_t) table, (uint64_t) payer, (uint64_t) id, (array_ptr<const char>) buffer, (uint32_t) buffer_size) )
{
   return context.db_store_i64( name(scope), name(table), account_name(payer), id, buffer, buffer_size );
}

SYSCALL_DEFINE( void, db_update_i64, ((int) itr, (uint64_t) payer, (array_ptr< const char >) buffer, (uint32_t) buffer_size) )
{
   context.db_update_i64( itr, account_name(payer), buffer, buffer_size );
}

SYSCALL_DEFINE( void, db_remove_i64, ((int) itr) )
{
   context.db_remove_i64( itr );
}

SYSCALL_DEFINE( int, db_get_i64, ((int) itr, (array_ptr< char >) buffer, (uint32_t) buffer_size) )
{
   return context.db_get_i64( itr, buffer, buffer_size );
}

SYSCALL_DEFINE( int, db_next_i64, ((int) itr, (uint64_t&) primary) )
{
   return context.db_next_i64(itr, primary);
}

SYSCALL_DEFINE( int, db_previous_i64, ((int) itr, (uint64_t&) primary) )
{
   return context.db_previous_i64(itr, primary);
}

SYSCALL_DEFINE( int, db_find_i64, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (uint64_t) id) )
{
   return context.db_find_i64( name(code), name(scope), name(table), id );
}

SYSCALL_DEFINE( int, db_lowerbound_i64, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (uint64_t) id) )
{
   return context.db_lowerbound_i64( name(code), name(scope), name(table), id );
}

SYSCALL_DEFINE( int, db_upperbound_i64, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (uint64_t) id) )
{
   return context.db_upperbound_i64( name(code), name(scope), name(table), id );
}

SYSCALL_DEFINE( int, db_end_i64, ((uint64_t) code, (uint64_t) scope, (uint64_t) table) )
{
   return context.db_end_i64( name(code), name(scope), name(table) );
}

DB_API_METHOD_WRAPPERS_SIMPLE_SECONDARY(idx64,  uint64_t)
DB_API_METHOD_WRAPPERS_SIMPLE_SECONDARY(idx128, uint128_t)
DB_API_METHOD_WRAPPERS_ARRAY_SECONDARY(idx256, 2, uint128_t)

class softfloat_api  {
   public:
      // TODO add traps on truncations for special cases (NaN or outside the range which rounds to an integer)
      softfloat_api( apply_context& ctx ){}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
      // float binops
      float _eosio_f32_add( float a, float b ) {
         float32_t ret = ::f32_add( to_softfloat32(a), to_softfloat32(b) );
         return *reinterpret_cast<float*>(&ret);
      }
      float _eosio_f32_sub( float a, float b ) {
         float32_t ret = ::f32_sub( to_softfloat32(a), to_softfloat32(b) );
         return *reinterpret_cast<float*>(&ret);
      }
      float _eosio_f32_div( float a, float b ) {
         float32_t ret = ::f32_div( to_softfloat32(a), to_softfloat32(b) );
         return *reinterpret_cast<float*>(&ret);
      }
      float _eosio_f32_mul( float a, float b ) {
         float32_t ret = ::f32_mul( to_softfloat32(a), to_softfloat32(b) );
         return *reinterpret_cast<float*>(&ret);
      }
#pragma GCC diagnostic pop
      float _eosio_f32_min( float af, float bf ) {
         float32_t a = to_softfloat32(af);
         float32_t b = to_softfloat32(bf);
         if (is_nan(a)) {
            return af;
         }
         if (is_nan(b)) {
            return bf;
         }
         if ( f32_sign_bit(a) != f32_sign_bit(b) ) {
            return f32_sign_bit(a) ? af : bf;
         }
         return ::f32_lt(a,b) ? af : bf;
      }
      float _eosio_f32_max( float af, float bf ) {
         float32_t a = to_softfloat32(af);
         float32_t b = to_softfloat32(bf);
         if (is_nan(a)) {
            return af;
         }
         if (is_nan(b)) {
            return bf;
         }
         if ( f32_sign_bit(a) != f32_sign_bit(b) ) {
            return f32_sign_bit(a) ? bf : af;
         }
         return ::f32_lt( a, b ) ? bf : af;
      }
      float _eosio_f32_copysign( float af, float bf ) {
         float32_t a = to_softfloat32(af);
         float32_t b = to_softfloat32(bf);
         uint32_t sign_of_b = b.v >> 31;
         a.v &= ~(1 << 31);             // clear the sign bit
         a.v = a.v | (sign_of_b << 31); // add the sign of b
         return from_softfloat32(a);
      }
      // float unops
      float _eosio_f32_abs( float af ) {
         float32_t a = to_softfloat32(af);
         a.v &= ~(1 << 31);
         return from_softfloat32(a);
      }
      float _eosio_f32_neg( float af ) {
         float32_t a = to_softfloat32(af);
         uint32_t sign = a.v >> 31;
         a.v &= ~(1 << 31);
         a.v |= (!sign << 31);
         return from_softfloat32(a);
      }
      float _eosio_f32_sqrt( float a ) {
         float32_t ret = ::f32_sqrt( to_softfloat32(a) );
         return from_softfloat32(ret);
      }
      // ceil, floor, trunc and nearest are lifted from libc
      float _eosio_f32_ceil( float af ) {
         float32_t a = to_softfloat32(af);
         int e = (int)(a.v >> 23 & 0xFF) - 0X7F;
         uint32_t m;
         if (e >= 23)
            return af;
         if (e >= 0) {
            m = 0x007FFFFF >> e;
            if ((a.v & m) == 0)
               return af;
            if (a.v >> 31 == 0)
               a.v += m;
            a.v &= ~m;
         } else {
            if (a.v >> 31)
               a.v = 0x80000000; // return -0.0f
            else if (a.v << 1)
               a.v = 0x3F800000; // return 1.0f
         }

         return from_softfloat32(a);
      }
      float _eosio_f32_floor( float af ) {
         float32_t a = to_softfloat32(af);
         int e = (int)(a.v >> 23 & 0xFF) - 0X7F;
         uint32_t m;
         if (e >= 23)
            return af;
         if (e >= 0) {
            m = 0x007FFFFF >> e;
            if ((a.v & m) == 0)
               return af;
            if (a.v >> 31)
               a.v += m;
            a.v &= ~m;
         } else {
            if (a.v >> 31 == 0)
               a.v = 0;
            else if (a.v << 1)
               a.v = 0xBF800000; // return -1.0f
         }
         return from_softfloat32(a);
      }
      float _eosio_f32_trunc( float af ) {
         float32_t a = to_softfloat32(af);
         int e = (int)(a.v >> 23 & 0xff) - 0x7f + 9;
         uint32_t m;
         if (e >= 23 + 9)
            return af;
         if (e < 9)
            e = 1;
         m = -1U >> e;
         if ((a.v & m) == 0)
            return af;
         a.v &= ~m;
         return from_softfloat32(a);
      }
      float _eosio_f32_nearest( float af ) {
         float32_t a = to_softfloat32(af);
         int e = a.v>>23 & 0xff;
         int s = a.v>>31;
         float32_t y;
         if (e >= 0x7f+23)
            return af;
         if (s)
            y = ::f32_add( ::f32_sub( a, float32_t{inv_float_eps} ), float32_t{inv_float_eps} );
         else
            y = ::f32_sub( ::f32_add( a, float32_t{inv_float_eps} ), float32_t{inv_float_eps} );
         if (::f32_eq( y, {0} ) )
            return s ? -0.0f : 0.0f;
         return from_softfloat32(y);
      }

      // float relops
      bool _eosio_f32_eq( float a, float b ) {  return ::f32_eq( to_softfloat32(a), to_softfloat32(b) ); }
      bool _eosio_f32_ne( float a, float b ) { return !::f32_eq( to_softfloat32(a), to_softfloat32(b) ); }
      bool _eosio_f32_lt( float a, float b ) { return ::f32_lt( to_softfloat32(a), to_softfloat32(b) ); }
      bool _eosio_f32_le( float a, float b ) { return ::f32_le( to_softfloat32(a), to_softfloat32(b) ); }
      bool _eosio_f32_gt( float af, float bf ) {
         float32_t a = to_softfloat32(af);
         float32_t b = to_softfloat32(bf);
         if (is_nan(a))
            return false;
         if (is_nan(b))
            return false;
         return !::f32_le( a, b );
      }
      bool _eosio_f32_ge( float af, float bf ) {
         float32_t a = to_softfloat32(af);
         float32_t b = to_softfloat32(bf);
         if (is_nan(a))
            return false;
         if (is_nan(b))
            return false;
         return !::f32_lt( a, b );
      }

      // double binops
      double _eosio_f64_add( double a, double b ) {
         float64_t ret = ::f64_add( to_softfloat64(a), to_softfloat64(b) );
         return from_softfloat64(ret);
      }
      double _eosio_f64_sub( double a, double b ) {
         float64_t ret = ::f64_sub( to_softfloat64(a), to_softfloat64(b) );
         return from_softfloat64(ret);
      }
      double _eosio_f64_div( double a, double b ) {
         float64_t ret = ::f64_div( to_softfloat64(a), to_softfloat64(b) );
         return from_softfloat64(ret);
      }
      double _eosio_f64_mul( double a, double b ) {
         float64_t ret = ::f64_mul( to_softfloat64(a), to_softfloat64(b) );
         return from_softfloat64(ret);
      }
      double _eosio_f64_min( double af, double bf ) {
         float64_t a = to_softfloat64(af);
         float64_t b = to_softfloat64(bf);
         if (is_nan(a))
            return af;
         if (is_nan(b))
            return bf;
         if (f64_sign_bit(a) != f64_sign_bit(b))
            return f64_sign_bit(a) ? af : bf;
         return ::f64_lt( a, b ) ? af : bf;
      }
      double _eosio_f64_max( double af, double bf ) {
         float64_t a = to_softfloat64(af);
         float64_t b = to_softfloat64(bf);
         if (is_nan(a))
            return af;
         if (is_nan(b))
            return bf;
         if (f64_sign_bit(a) != f64_sign_bit(b))
            return f64_sign_bit(a) ? bf : af;
         return ::f64_lt( a, b ) ? bf : af;
      }
      double _eosio_f64_copysign( double af, double bf ) {
         float64_t a = to_softfloat64(af);
         float64_t b = to_softfloat64(bf);
         uint64_t sign_of_b = b.v >> 63;
         a.v &= ~(uint64_t(1) << 63);             // clear the sign bit
         a.v = a.v | (sign_of_b << 63); // add the sign of b
         return from_softfloat64(a);
      }

      // double unops
      double _eosio_f64_abs( double af ) {
         float64_t a = to_softfloat64(af);
         a.v &= ~(uint64_t(1) << 63);
         return from_softfloat64(a);
      }
      double _eosio_f64_neg( double af ) {
         float64_t a = to_softfloat64(af);
         uint64_t sign = a.v >> 63;
         a.v &= ~(uint64_t(1) << 63);
         a.v |= (uint64_t(!sign) << 63);
         return from_softfloat64(a);
      }
      double _eosio_f64_sqrt( double a ) {
         float64_t ret = ::f64_sqrt( to_softfloat64(a) );
         return from_softfloat64(ret);
      }
      // ceil, floor, trunc and nearest are lifted from libc
      double _eosio_f64_ceil( double af ) {
         float64_t a = to_softfloat64( af );
         float64_t ret;
         int e = a.v >> 52 & 0x7ff;
         float64_t y;
         if (e >= 0x3ff+52 || ::f64_eq( a, { 0 } ))
            return af;
         /* y = int(x) - x, where int(x) is an integer neighbor of x */
         if (a.v >> 63)
            y = ::f64_sub( ::f64_add( ::f64_sub( a, float64_t{inv_double_eps} ), float64_t{inv_double_eps} ), a );
         else
            y = ::f64_sub( ::f64_sub( ::f64_add( a, float64_t{inv_double_eps} ), float64_t{inv_double_eps} ), a );
         /* special case because of non-nearest rounding modes */
         if (e <= 0x3ff-1) {
            return a.v >> 63 ? -0.0 : 1.0; //float64_t{0x8000000000000000} : float64_t{0xBE99999A3F800000}; //either -0.0 or 1
         }
         if (::f64_lt( y, to_softfloat64(0) )) {
            ret = ::f64_add( ::f64_add( a, y ), to_softfloat64(1) ); // 0xBE99999A3F800000 } ); // plus 1
            return from_softfloat64(ret);
         }
         ret = ::f64_add( a, y );
         return from_softfloat64(ret);
      }
      double _eosio_f64_floor( double af ) {
         float64_t a = to_softfloat64( af );
         float64_t ret;
         int e = a.v >> 52 & 0x7FF;
         float64_t y;
         if ( a.v == 0x8000000000000000) {
            return af;
         }
         if (e >= 0x3FF+52 || a.v == 0) {
            return af;
         }
         if (a.v >> 63)
            y = ::f64_sub( ::f64_add( ::f64_sub( a, float64_t{inv_double_eps} ), float64_t{inv_double_eps} ), a );
         else
            y = ::f64_sub( ::f64_sub( ::f64_add( a, float64_t{inv_double_eps} ), float64_t{inv_double_eps} ), a );
         if (e <= 0x3FF-1) {
            return a.v>>63 ? -1.0 : 0.0; //float64_t{0xBFF0000000000000} : float64_t{0}; // -1 or 0
         }
         if ( !::f64_le( y, float64_t{0} ) ) {
            ret = ::f64_sub( ::f64_add(a,y), to_softfloat64(1.0));
            return from_softfloat64(ret);
         }
         ret = ::f64_add( a, y );
         return from_softfloat64(ret);
      }
      double _eosio_f64_trunc( double af ) {
         float64_t a = to_softfloat64( af );
         int e = (int)(a.v >> 52 & 0x7ff) - 0x3ff + 12;
         uint64_t m;
         if (e >= 52 + 12)
            return af;
         if (e < 12)
            e = 1;
         m = -1ULL >> e;
         if ((a.v & m) == 0)
            return af;
         a.v &= ~m;
         return from_softfloat64(a);
      }

      double _eosio_f64_nearest( double af ) {
         float64_t a = to_softfloat64( af );
         int e = (a.v >> 52 & 0x7FF);
         int s = a.v >> 63;
         float64_t y;
         if ( e >= 0x3FF+52 )
            return af;
         if ( s )
            y = ::f64_add( ::f64_sub( a, float64_t{inv_double_eps} ), float64_t{inv_double_eps} );
         else
            y = ::f64_sub( ::f64_add( a, float64_t{inv_double_eps} ), float64_t{inv_double_eps} );
         if ( ::f64_eq( y, float64_t{0} ) )
            return s ? -0.0 : 0.0;
         return from_softfloat64(y);
      }

      // double relops
      bool _eosio_f64_eq( double a, double b ) { return ::f64_eq( to_softfloat64(a), to_softfloat64(b) ); }
      bool _eosio_f64_ne( double a, double b ) { return !::f64_eq( to_softfloat64(a), to_softfloat64(b) ); }
      bool _eosio_f64_lt( double a, double b ) { return ::f64_lt( to_softfloat64(a), to_softfloat64(b) ); }
      bool _eosio_f64_le( double a, double b ) { return ::f64_le( to_softfloat64(a), to_softfloat64(b) ); }
      bool _eosio_f64_gt( double af, double bf ) {
         float64_t a = to_softfloat64(af);
         float64_t b = to_softfloat64(bf);
         if (is_nan(a))
            return false;
         if (is_nan(b))
            return false;
         return !::f64_le( a, b );
      }
      bool _eosio_f64_ge( double af, double bf ) {
         float64_t a = to_softfloat64(af);
         float64_t b = to_softfloat64(bf);
         if (is_nan(a))
            return false;
         if (is_nan(b))
            return false;
         return !::f64_lt( a, b );
      }

      // float and double conversions
      double _eosio_f32_promote( float a ) {
         return from_softfloat64(f32_to_f64( to_softfloat32(a)) );
      }
      float _eosio_f64_demote( double a ) {
         return from_softfloat32(f64_to_f32( to_softfloat64(a)) );
      }
      int32_t _eosio_f32_trunc_i32s( float af ) {
         float32_t a = to_softfloat32(af);
         if (_eosio_f32_ge(af, 2147483648.0f) || _eosio_f32_lt(af, -2147483648.0f))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_s/i32 overflow" );

         if (is_nan(a))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_s/i32 unrepresentable");
         return f32_to_i32( to_softfloat32(_eosio_f32_trunc( af )), 0, false );
      }
      int32_t _eosio_f64_trunc_i32s( double af ) {
         float64_t a = to_softfloat64(af);
         if (_eosio_f64_ge(af, 2147483648.0) || _eosio_f64_lt(af, -2147483648.0))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_s/i32 overflow");
         if (is_nan(a))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_s/i32 unrepresentable");
         return f64_to_i32( to_softfloat64(_eosio_f64_trunc( af )), 0, false );
      }
      uint32_t _eosio_f32_trunc_i32u( float af ) {
         float32_t a = to_softfloat32(af);
         if (_eosio_f32_ge(af, 4294967296.0f) || _eosio_f32_le(af, -1.0f))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_u/i32 overflow");
         if (is_nan(a))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_u/i32 unrepresentable");
         return f32_to_ui32( to_softfloat32(_eosio_f32_trunc( af )), 0, false );
      }
      uint32_t _eosio_f64_trunc_i32u( double af ) {
         float64_t a = to_softfloat64(af);
         if (_eosio_f64_ge(af, 4294967296.0) || _eosio_f64_le(af, -1.0))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_u/i32 overflow");
         if (is_nan(a))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_u/i32 unrepresentable");
         return f64_to_ui32( to_softfloat64(_eosio_f64_trunc( af )), 0, false );
      }
      int64_t _eosio_f32_trunc_i64s( float af ) {
         float32_t a = to_softfloat32(af);
         if (_eosio_f32_ge(af, 9223372036854775808.0f) || _eosio_f32_lt(af, -9223372036854775808.0f))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_s/i64 overflow");
         if (is_nan(a))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_s/i64 unrepresentable");
         return f32_to_i64( to_softfloat32(_eosio_f32_trunc( af )), 0, false );
      }
      int64_t _eosio_f64_trunc_i64s( double af ) {
         float64_t a = to_softfloat64(af);
         if (_eosio_f64_ge(af, 9223372036854775808.0) || _eosio_f64_lt(af, -9223372036854775808.0))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_s/i64 overflow");
         if (is_nan(a))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_s/i64 unrepresentable");

         return f64_to_i64( to_softfloat64(_eosio_f64_trunc( af )), 0, false );
      }
      uint64_t _eosio_f32_trunc_i64u( float af ) {
         float32_t a = to_softfloat32(af);
         if (_eosio_f32_ge(af, 18446744073709551616.0f) || _eosio_f32_le(af, -1.0f))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_u/i64 overflow");
         if (is_nan(a))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f32.convert_u/i64 unrepresentable");
         return f32_to_ui64( to_softfloat32(_eosio_f32_trunc( af )), 0, false );
      }
      uint64_t _eosio_f64_trunc_i64u( double af ) {
         float64_t a = to_softfloat64(af);
         if (_eosio_f64_ge(af, 18446744073709551616.0) || _eosio_f64_le(af, -1.0))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_u/i64 overflow");
         if (is_nan(a))
            KOINOS_THROW( koinos::chain::wasm_execution_error, "Error, f64.convert_u/i64 unrepresentable");
         return f64_to_ui64( to_softfloat64(_eosio_f64_trunc( af )), 0, false );
      }
      float _eosio_i32_to_f32( int32_t a )  {
         return from_softfloat32(i32_to_f32( a ));
      }
      float _eosio_i64_to_f32( int64_t a ) {
         return from_softfloat32(i64_to_f32( a ));
      }
      float _eosio_ui32_to_f32( uint32_t a ) {
         return from_softfloat32(ui32_to_f32( a ));
      }
      float _eosio_ui64_to_f32( uint64_t a ) {
         return from_softfloat32(ui64_to_f32( a ));
      }
      double _eosio_i32_to_f64( int32_t a ) {
         return from_softfloat64(i32_to_f64( a ));
      }
      double _eosio_i64_to_f64( int64_t a ) {
         return from_softfloat64(i64_to_f64( a ));
      }
      double _eosio_ui32_to_f64( uint32_t a ) {
         return from_softfloat64(ui32_to_f64( a ));
      }
      double _eosio_ui64_to_f64( uint64_t a ) {
         return from_softfloat64(ui64_to_f64( a ));
      }

      static bool is_nan( const float32_t f ) {
         return f32_is_nan( f );
      }
      static bool is_nan( const float64_t f ) {
         return f64_is_nan( f );
      }
      static bool is_nan( const float128_t& f ) {
         return f128_is_nan( f );
      }

      static constexpr uint32_t inv_float_eps = 0x4B000000;
      static constexpr uint64_t inv_double_eps = 0x4330000000000000;
};

DB_API_METHOD_WRAPPERS_FLOAT_SECONDARY(idx_double, float64_t)
DB_API_METHOD_WRAPPERS_FLOAT_SECONDARY(idx_long_double, float128_t)


} // koinos::chain
