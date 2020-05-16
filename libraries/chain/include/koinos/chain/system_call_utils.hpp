#pragma once
#include <boost/preprocessor.hpp>

#define SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE "cannot be called directly from user mode"

#define SYSTEM_CALL_SLOT(s) s, BOOST_PP_CAT(_,s)

#define SYSTEM_CALL_DECLARE(return_type, name, ...) \
   return_type name(__VA_ARGS__);               \
   return_type BOOST_PP_CAT(_,name)(__VA_ARGS__)

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
#define SYSTEM_CALL_DEFINE( RETURN_TYPE, SYSCALL, ... )                                       \
   RETURN_TYPE system_api::SYSCALL( DETAIL_DEFINE_ARGS(__VA_ARGS__) )                         \
   {                                                                                          \
      BOOST_PP_IF(IS_VOID(RETURN_TYPE),,RETURN_TYPE retval;)                                  \
      auto current_level = context.privilege_level;                                           \
      context.privilege_level = privilege::kernel_mode;                                       \
      try                                                                                     \
      {                                                                                       \
         auto call_override = context.syscalls.get_system_call( system_call_slot::SYSCALL );  \
         if ( call_override )                                                                 \
         {                                                                                    \
         }                                                                                    \
         else                                                                                 \
         {                                                                                    \
            BOOST_PP_IF(                                                                      \
               IS_VOID(RETURN_TYPE),                                                          \
               SYSCALL_INTERNAL_CALL(SYSCALL, __VA_ARGS__),                                   \
               SYSCALL_INTERNAL_CALL_WITH_RETURN(retval, SYSCALL, __VA_ARGS__)                \
            )                                                                                 \
         }                                                                                    \
      }                                                                                       \
      catch ( ... )                                                                           \
      {                                                                                       \
         context.privilege_level = current_level;                                             \
         throw;                                                                               \
      }                                                                                       \
      context.privilege_level = current_level;                                                \
      BOOST_PP_IF(IS_VOID(RETURN_TYPE),,return retval;)                                       \
   }                                                                                          \
   RETURN_TYPE system_api::BOOST_PP_CAT(_,SYSCALL)( DETAIL_DEFINE_ARGS(__VA_ARGS__) )

#define DB_API_METHOD_WRAPPERS_SIMPLE_SECONDARY(IDX, TYPE)\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_store, ((uint64_t) scope, (uint64_t) table, (uint64_t) payer, (uint64_t) id, (const TYPE&) secondary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.store( scope, table, account_name(payer), id, secondary );\
   }\
   SYSTEM_CALL_DEFINE( void, db_##IDX##_update, ((int) iterator, (uint64_t) payer, (const TYPE&) secondary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.update( iterator, account_name(payer), secondary );\
   }\
   SYSTEM_CALL_DEFINE( void, db_##IDX##_remove, ((int) iterator ) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.remove( iterator );\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_find_secondary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (const TYPE&) secondary, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.find_secondary(code, scope, table, secondary, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_find_primary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (TYPE&) secondary, (uint64_t) primary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.find_primary(code, scope, table, secondary, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_lowerbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (TYPE&) secondary, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.lowerbound_secondary(code, scope, table, secondary, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_upperbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (TYPE&) secondary, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.upperbound_secondary(code, scope, table, secondary, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_end, ((uint64_t) code, (uint64_t) scope, (uint64_t) table) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.end_secondary(code, scope, table);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_next, ((int) iterator, (uint64_t&) primary)  ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.next_secondary(iterator, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_previous, ((int) iterator, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.previous_secondary(iterator, primary);\
   }

#define DB_API_METHOD_WRAPPERS_ARRAY_SECONDARY(IDX, ARR_SIZE, ARR_ELEMENT_TYPE)\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_store, ((uint64_t) scope, (uint64_t) table, (uint64_t) payer, (uint64_t) id, (array_ptr<const ARR_ELEMENT_TYPE>) data, (uint32_t) data_len) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
         return context.IDX.store(scope, table, account_name(payer), id, data.value);\
   }\
   SYSTEM_CALL_DEFINE( void, db_##IDX##_update, ((int) iterator, (uint64_t) payer, (array_ptr<const ARR_ELEMENT_TYPE>) data, (uint32_t) data_len) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
      return context.IDX.update(iterator, account_name(payer), data.value);\
   }\
   SYSTEM_CALL_DEFINE( void, db_##IDX##_remove, ((int) iterator ) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.remove( iterator );\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_find_secondary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (array_ptr<const ARR_ELEMENT_TYPE>) data, (uint32_t) data_len, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
      return context.IDX.find_secondary(code, scope, table, data, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_find_primary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (array_ptr<ARR_ELEMENT_TYPE>) data, (uint32_t) data_len, (uint64_t) primary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
      return context.IDX.find_primary(code, scope, table, data.value, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_lowerbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (array_ptr<ARR_ELEMENT_TYPE>) data, (uint32_t) data_len, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
      return context.IDX.lowerbound_secondary(code, scope, table, data.value, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_upperbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (array_ptr<ARR_ELEMENT_TYPE>) data, (uint32_t) data_len, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      KOINOS_ASSERT( data_len == ARR_SIZE,\
                    db_api_exception,\
                    "invalid size of secondary key array for " #IDX ": given ${given} bytes but expected ${expected} bytes",\
                    ("given",data_len)("expected",ARR_SIZE) );\
      return context.IDX.upperbound_secondary(code, scope, table, data.value, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_end, ((uint64_t) code, (uint64_t) scope, (uint64_t) table) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.end_secondary(code, scope, table);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_next, ((int) iterator, (uint64_t&) primary)  ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.next_secondary(iterator, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_previous, ((int) iterator, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.previous_secondary(iterator, primary);\
   }

#define DB_API_METHOD_WRAPPERS_FLOAT_SECONDARY(IDX, TYPE)\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_store, ((uint64_t) scope, (uint64_t) table, (uint64_t) payer, (uint64_t) id, (const TYPE&) secondary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      KOINOS_ASSERT( !softfloat_api::is_nan( secondary ), transaction_exception, "NaN is not an allowed value for a secondary key" );\
      return context.IDX.store( scope, table, account_name(payer), id, secondary );\
   }\
   SYSTEM_CALL_DEFINE( void, db_##IDX##_update, ((int) iterator, (uint64_t) payer, (const TYPE&) secondary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      KOINOS_ASSERT( !softfloat_api::is_nan( secondary ), transaction_exception, "NaN is not an allowed value for a secondary key" );\
      return context.IDX.update( iterator, account_name(payer), secondary );\
   }\
   SYSTEM_CALL_DEFINE( void, db_##IDX##_remove, ((int) iterator ) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.remove( iterator );\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_find_secondary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (const TYPE&) secondary, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      KOINOS_ASSERT( !softfloat_api::is_nan( secondary ), transaction_exception, "NaN is not an allowed value for a secondary key" );\
      return context.IDX.find_secondary(code, scope, table, secondary, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_find_primary, ((uint64_t) code, (uint64_t) scope, (uint64_t) table, (TYPE&) secondary, (uint64_t) primary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.find_primary(code, scope, table, secondary, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_lowerbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (TYPE&) secondary, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      KOINOS_ASSERT( !softfloat_api::is_nan( secondary ), transaction_exception, "NaN is not an allowed value for a secondary key" );\
      return context.IDX.lowerbound_secondary(code, scope, table, secondary, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_upperbound, ((uint64_t) code, (uint64_t) scope, (uint64_t) table,  (TYPE&) secondary, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      KOINOS_ASSERT( !softfloat_api::is_nan( secondary ), transaction_exception, "NaN is not an allowed value for a secondary key" );\
      return context.IDX.upperbound_secondary(code, scope, table, secondary, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_end, ((uint64_t) code, (uint64_t) scope, (uint64_t) table) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.end_secondary(code, scope, table);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_next, ((int) iterator, (uint64_t&) primary)  ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.next_secondary(iterator, primary);\
   }\
   SYSTEM_CALL_DEFINE( int, db_##IDX##_previous, ((int) iterator, (uint64_t&) primary) ) {\
      KOINOS_ASSERT( context.privilege_level == privilege::kernel_mode, insufficient_privileges, SYSTEM_CALL_INSUFFICENT_PRIVILEGE_MESSAGE );\
      return context.IDX.previous_secondary(iterator, primary);\
   }
