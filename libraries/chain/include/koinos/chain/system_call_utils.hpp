#pragma once
#include <boost/preprocessor.hpp>

// This file exposes seven public macros for consumption
// 1. SYSTEM_CALL_SLOTS
// 2. SYSTEM_CALL_DECLARE
// 3. SYSTEM_CALL_DEFINE
// 4. SYSTEM_CALL_ENFORCE_KERNEL_MODE
// 5. SYSTEM_CALL_DB_WRAPPERS_SIMPLE_SECONDARY
// 6. SYSTEM_CALL_DB_WRAPPERS_ARRAY_SECONDARY
// 7. SYSTEM_CALL_DB_WRAPPERS_FLOAT_SECONDARY

#define _SYSCALL_PRIVATE_PREFIX internal_

#define _SYSCALL_SLOT(r, data, i, elem) BOOST_PP_COMMA_IF(i) elem, BOOST_PP_CAT(_SYSCALL_PRIVATE_PREFIX,elem)

#define _SYSCALL_SLOT_REGISTRATION(r, data, i, elem) \
koinos::chain::registrar_type::add< system_api, &system_api::elem, koinos::chain::wasm_allocator_type >( "env", BOOST_PP_STRINGIZE(elem) );

#define SYSTEM_CALL_SLOTS( args )                                 \
inline void register_syscalls()                                   \
{                                                                 \
   BOOST_PP_SEQ_FOR_EACH_I(_SYSCALL_SLOT_REGISTRATION, =>, args ) \
}                                                                 \
enum class system_call_slot : uint32_t                            \
{                                                                 \
   BOOST_PP_SEQ_FOR_EACH_I(_SYSCALL_SLOT, =>, args )              \
}

#define SYSTEM_CALL_ENFORCE_KERNEL_MODE() KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, "cannot be called directly from user mode" );

#define SYSTEM_CALL_DECLARE(return_type, name, ...)                    \
   return_type name(__VA_ARGS__);                                      \
   return_type BOOST_PP_CAT(_SYSCALL_PRIVATE_PREFIX,name)(__VA_ARGS__)

#define _SYSCALL_RET_TYPE_void 1)(1
#define _SYSCALL_IS_VOID(type) BOOST_PP_EQUAL(BOOST_PP_SEQ_SIZE((BOOST_PP_CAT(_SYSCALL_RET_TYPE_,type))),2)

#define _SYSCALL_REM(...) __VA_ARGS__
#define _SYSCALL_EAT(...)

// Retrieve the type
#define _SYSCALL_TYPEOF(x) _SYSCALL_DETAIL_TYPEOF(_SYSCALL_DETAIL_TYPEOF_PROBE x,)
#define _SYSCALL_DETAIL_TYPEOF(...) _SYSCALL_DETAIL_TYPEOF_HEAD(__VA_ARGS__)
#define _SYSCALL_DETAIL_TYPEOF_HEAD(x, ...) _SYSCALL_REM x
#define _SYSCALL_DETAIL_TYPEOF_PROBE(...) (__VA_ARGS__),
// Strip off the type
#define _SYSCALL_STRIP(x) _SYSCALL_EAT x
// Show the type without parenthesis
#define _SYSCALL_PAIR(x) _SYSCALL_REM x

#define _SYSCALL_DETAIL_DEFINE_MEMBERS_EACH(r, data, x) _SYSCALL_PAIR(x);
#define _SYSCALL_DETAIL_DEFINE_ARGS_EACH(r, data, i, x) BOOST_PP_COMMA_IF(i) _SYSCALL_PAIR(x)
#define _SYSCALL_DETAIL_DEFINE_FORWARD_EACH(r, data, i, x) BOOST_PP_COMMA_IF(i) _SYSCALL_STRIP(x)

#define _SYSCALL_DETAIL_DEFINE_MEMBERS(args) BOOST_PP_SEQ_FOR_EACH(_SYSCALL_DETAIL_DEFINE_MEMBERS_EACH, data, BOOST_PP_VARIADIC_TO_SEQ args)
#define _SYSCALL_DETAIL_DEFINE_ARGS(args) BOOST_PP_SEQ_FOR_EACH_I(_SYSCALL_DETAIL_DEFINE_ARGS_EACH, data, BOOST_PP_VARIADIC_TO_SEQ args)
#define _SYSCALL_DETAIL_DEFINE_FORWARD(args) BOOST_PP_SEQ_FOR_EACH_I(_SYSCALL_DETAIL_DEFINE_FORWARD_EACH, data, BOOST_PP_VARIADIC_TO_SEQ args)

#define _SYSCALL_INTERNAL_CALL(x, ...) BOOST_PP_CAT(_SYSCALL_PRIVATE_PREFIX,x)(_SYSCALL_DETAIL_DEFINE_FORWARD(__VA_ARGS__));
#define _SYSCALL_INTERNAL_CALL_WITH_RETURN(ret, x, ...) ret = _SYSCALL_INTERNAL_CALL(x, __VA_ARGS__);

#pragma message( "TODO: Use the syscall_bundle to actually call VM with arguments and retrieve return value" )
#define SYSTEM_CALL_DEFINE( RETURN_TYPE, SYSCALL, ... )                                       \
   RETURN_TYPE system_api::SYSCALL( _SYSCALL_DETAIL_DEFINE_ARGS(__VA_ARGS__) )                \
   {                                                                                          \
      BOOST_PP_IF(_SYSCALL_IS_VOID(RETURN_TYPE),,RETURN_TYPE retval;)                         \
      auto current_level = context.privilege_level;                                           \
      context.privilege_level = privilege::kernel_mode;                                       \
      try                                                                                     \
      {                                                                                       \
         auto call_override = context.syscalls.get_system_call( system_call_slot::SYSCALL );  \
         if ( call_override )                                                                 \
         {                                                                                    \
            BOOST_PP_IF(                                                                      \
               _SYSCALL_IS_VOID(RETURN_TYPE),                                                 \
               _SYSCALL_INTERNAL_CALL(SYSCALL, __VA_ARGS__),                                  \
               _SYSCALL_INTERNAL_CALL_WITH_RETURN(retval, SYSCALL, __VA_ARGS__)               \
            )                                                                                 \
         }                                                                                    \
         else                                                                                 \
         {                                                                                    \
            BOOST_PP_IF(                                                                      \
               _SYSCALL_IS_VOID(RETURN_TYPE),                                                 \
               _SYSCALL_INTERNAL_CALL(SYSCALL, __VA_ARGS__),                                  \
               _SYSCALL_INTERNAL_CALL_WITH_RETURN(retval, SYSCALL, __VA_ARGS__)               \
            )                                                                                 \
         }                                                                                    \
      }                                                                                       \
      catch ( ... )                                                                           \
      {                                                                                       \
         context.privilege_level = current_level;                                             \
         throw;                                                                               \
      }                                                                                       \
      context.privilege_level = current_level;                                                \
      BOOST_PP_IF(_SYSCALL_IS_VOID(RETURN_TYPE),,return retval;)                              \
   }                                                                                          \
   RETURN_TYPE system_api::BOOST_PP_CAT(_SYSCALL_PRIVATE_PREFIX,SYSCALL)( _SYSCALL_DETAIL_DEFINE_ARGS(__VA_ARGS__) )

#define SYSTEM_CALL_DB_WRAPPERS_SIMPLE_SECONDARY(IDX, TYPE)\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_store, ((uint64_t) scope, (uint64_t) table, (uint64_t) payer, (uint64_t) id, (const TYPE&) secondary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.store( scope, table, account_name(payer), id, secondary );\
   }\
   SYSTEM_CALL_DEFINE( void, db_##IDX##_update, ((int) iterator, (uint64_t) payer, (const TYPE&) secondary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.update( iterator, account_name(payer), secondary );\
   }\
   SYSTEM_CALL_DEFINE( void, db_##IDX##_remove, ((int) iterator ) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.remove( iterator );\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_find_secondary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (const TYPE&) secondary, (uint64_t&) primary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.find_secondary(code, scope, table, secondary, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_find_primary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (TYPE&) secondary, (uint64_t) primary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.find_primary(code, scope, table, secondary, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_lowerbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (TYPE&) secondary, (uint64_t&) primary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.lowerbound_secondary(code, scope, table, secondary, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_upperbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (TYPE&) secondary, (uint64_t&) primary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.upperbound_secondary(code, scope, table, secondary, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_end, ((uint64_t) code, (uint64_t) scope, (uint64_t) table) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.end_secondary(code, scope, table);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_next, ((int) iterator, (uint64_t&) primary)  ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.next_secondary(iterator, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_previous, ((int) iterator, (uint64_t&) primary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.previous_secondary(iterator, primary);\
   }

#define SYSTEM_CALL_DB_WRAPPERS_ARRAY_SECONDARY(IDX, ARR_SIZE, ARR_ELEMENT_TYPE)\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_store, ((uint64_t) scope, (uint64_t) table, (uint64_t) payer, (uint64_t) id, (array_ptr<const ARR_ELEMENT_TYPE>) data, (uint32_t) data_len) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
         return context.IDX.store(scope, table, account_name(payer), id, data.value);\
   }\
   SYSTEM_CALL_DEFINE( void, db_##IDX##_update, ((int) iterator, (uint64_t) payer, (array_ptr<const ARR_ELEMENT_TYPE>) data, (uint32_t) data_len) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
      return context.IDX.update(iterator, account_name(payer), data.value);\
   }\
   SYSTEM_CALL_DEFINE( void, db_##IDX##_remove, ((int) iterator ) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.remove( iterator );\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_find_secondary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (array_ptr<const ARR_ELEMENT_TYPE>) data, (uint32_t) data_len, (uint64_t&) primary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
      return context.IDX.find_secondary(code, scope, table, data, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_find_primary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (array_ptr<ARR_ELEMENT_TYPE>) data, (uint32_t) data_len, (uint64_t) primary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
      return context.IDX.find_primary(code, scope, table, data.value, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_lowerbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (array_ptr<ARR_ELEMENT_TYPE>) data, (uint32_t) data_len, (uint64_t&) primary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
      return context.IDX.lowerbound_secondary(code, scope, table, data.value, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_upperbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (array_ptr<ARR_ELEMENT_TYPE>) data, (uint32_t) data_len, (uint64_t&) primary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
      return context.IDX.upperbound_secondary(code, scope, table, data.value, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_end, ((uint64_t) code, (uint64_t) scope, (uint64_t) table) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.end_secondary(code, scope, table);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_next, ((int) iterator, (uint64_t&) primary)  ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.next_secondary(iterator, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_previous, ((int) iterator, (uint64_t&) primary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.previous_secondary(iterator, primary);\
   }

#define SYSTEM_CALL_DB_WRAPPERS_FLOAT_SECONDARY(IDX, TYPE)\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_store, ((uint64_t) scope, (uint64_t) table, (uint64_t) payer, (uint64_t) id, (const TYPE&) secondary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      KOINOS_ASSERT( !detail::soft_float::is_nan( secondary ), transaction_exception, "NaN is not an allowed value for a secondary key" );\
      return context.IDX.store( scope, table, account_name(payer), id, secondary );\
   }\
   SYSTEM_CALL_DEFINE( void, db_##IDX##_update, ((int) iterator, (uint64_t) payer, (const TYPE&) secondary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      KOINOS_ASSERT( !detail::soft_float::is_nan( secondary ), transaction_exception, "NaN is not an allowed value for a secondary key" );\
      return context.IDX.update( iterator, account_name(payer), secondary );\
   }\
   SYSTEM_CALL_DEFINE( void, db_##IDX##_remove, ((int) iterator ) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.remove( iterator );\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_find_secondary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (const TYPE&) secondary, (uint64_t&) primary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      KOINOS_ASSERT( !detail::soft_float::is_nan( secondary ), transaction_exception, "NaN is not an allowed value for a secondary key" );\
      return context.IDX.find_secondary(code, scope, table, secondary, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_find_primary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (TYPE&) secondary, (uint64_t) primary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.find_primary(code, scope, table, secondary, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_lowerbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (TYPE&) secondary, (uint64_t&) primary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      KOINOS_ASSERT( !detail::soft_float::is_nan( secondary ), transaction_exception, "NaN is not an allowed value for a secondary key" );\
      return context.IDX.lowerbound_secondary(code, scope, table, secondary, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_upperbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (TYPE&) secondary, (uint64_t&) primary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      KOINOS_ASSERT( !detail::soft_float::is_nan( secondary ), transaction_exception, "NaN is not an allowed value for a secondary key" );\
      return context.IDX.upperbound_secondary(code, scope, table, secondary, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_end, ((uint64_t) code, (uint64_t) scope, (uint64_t) table) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.end_secondary(code, scope, table);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_next, ((int) iterator, (uint64_t&) primary)  ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.next_secondary(iterator, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_previous, ((int) iterator, (uint64_t&) primary) ) {\
      SYSTEM_CALL_ENFORCE_KERNEL_MODE();\
      return context.IDX.previous_secondary(iterator, primary);\
   }
