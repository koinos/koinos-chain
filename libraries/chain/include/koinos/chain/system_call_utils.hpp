#pragma once
#include <boost/preprocessor.hpp>
#include <boost/preprocessor/punctuation/comma.hpp>

#include <koinos/util.hpp>

// This file exposes seven public macros for consumption
// 1. SYSTEM_CALL_SLOTS
// 2. SYSTEM_CALL_DECLARE
// 3. SYSTEM_CALL_DEFINE
// 4. SYSTEM_CALL_ENFORCE_KERNEL_MODE
// 5. SYSTEM_CALL_DB_WRAPPERS_SIMPLE_SECONDARY
// 6. SYSTEM_CALL_DB_WRAPPERS_ARRAY_SECONDARY
// 7. SYSTEM_CALL_DB_WRAPPERS_FLOAT_SECONDARY

#define _THUNK_SUFFIX _thunk
#define _THUNK_ID_SUFFIX _thunk_id

#define SYSTEM_CALL_ENFORCE_KERNEL_MODE()

#define SYSTEM_CALL_DECLARE(return_type, name, ...)                           \
   return_type name( apply_context&, __VA_ARGS__);                            \
   return_type BOOST_PP_CAT(name,_THUNK_SUFFIX)( apply_context&, __VA_ARGS__)

#define _THUNK_RET_TYPE_void 1)(1
#define _THUNK_IS_VOID(type) BOOST_PP_EQUAL(BOOST_PP_SEQ_SIZE((BOOST_PP_CAT(_THUNK_RET_TYPE_,type))),2)

#define _SYSCALL_REM(...) __VA_ARGS__
#define _SYSCALL_EAT(...)

// Strip off the type
#define _SYSCALL_STRIP(x) _SYSCALL_EAT x
// Show the type without parenthesis
#define _SYSCALL_PAIR(x) _SYSCALL_REM x

#define _SYSCALL_DETAIL_DEFINE_ARGS_EACH(r, data, i, x) BOOST_PP_COMMA_IF(i) _SYSCALL_PAIR(x)
#define _SYSCALL_DETAIL_DEFINE_FORWARD_EACH(r, data, i, x) BOOST_PP_COMMA_IF(i) _SYSCALL_STRIP(x)

#define _SYSCALL_DETAIL_DEFINE_ARGS(args) BOOST_PP_SEQ_FOR_EACH_I(_SYSCALL_DETAIL_DEFINE_ARGS_EACH, data, BOOST_PP_VARIADIC_TO_SEQ args)
#define _SYSCALL_DETAIL_DEFINE_FORWARD(args) BOOST_PP_SEQ_FOR_EACH_I(_SYSCALL_DETAIL_DEFINE_FORWARD_EACH, data, BOOST_PP_VARIADIC_TO_SEQ args)

#define _THUNK_CALL(SYSCALL, tid, context, ...) thunk_dispatcher::instance().call_thunk< decltype(system_api::SYSCALL) >( tid BOOST_PP_COMMA context BOOST_PP_COMMA _SYSCALL_DETAIL_DEFINE_FORWARD(__VA_ARGS__) );
#define _THUNK_CALL_WITH_RETURN(ret, SYSCALL, tid, context, ...) ret = thunk_dispatcher::instance().call_thunk< decltype(ret) BOOST_PP_COMMA decltype(system_api::SYSCALL) >( tid BOOST_PP_COMMA context BOOST_PP_COMMA _SYSCALL_DETAIL_DEFINE_FORWARD(__VA_ARGS__) );

#define _THUNK_CALL_RETURN_TYPE( ret ) BOOST_PP_COMMA() ret

/*thunk_dispatcher::instance().call_thunk< decltype(SYSCALL), RETURN_TYPE >(                      \
                  tid,                                                                                         \
                  context,                                                                                     \
                  ret,                                                                                         \
                  _SYSCALL_DETAIL_DEFINE_FORWARD(__VA_ARGS__)                                                  \
               )*/

/*BOOST_PP_IF(                                                                                    \
                  _THUNK_IS_VOID(RETURN_TYPE),                                                                 \
                  _THUNK_CALL(SYSCALL, tid, context, __VA_ARGS__),                                             \
                  _THUNK_CALL_WITH_RETURN(ret, SYSCALL, tid, context, __VA_ARGS__)                             \
               )*/

#pragma message( "TODO:  Invoke smart contract xcall handler" )
#define SYSTEM_CALL_DEFINE( RETURN_TYPE, SYSCALL, ... )                                                        \
   RETURN_TYPE SYSCALL( apply_context& context, _SYSCALL_DETAIL_DEFINE_ARGS(__VA_ARGS__) )         \
   {                                                                                                           \
      using koinos::protocol::thunk_id_type;                                                                   \
      using koinos::protocol::contract_id_type;                                                                \
                                                                                                               \
      uint32_t _xid = BOOST_PP_CAT(SYSCALL,_THUNK_ID_SUFFIX);                                                   \
                                                                                                               \
      /* TODO Do we need to invoke serialization here? */                                                      \
      statedb::object_key _key = _xid;                                                                           \
                                                                                                               \
      koinos::protocol::vl_blob _vl_target =                                                                    \
         db_get_object_thunk( context, XCALL_DISPATCH_TABLE_SPACE_ID, _key, XCALL_DISPATCH_TABLE_OBJECT_MAX_SIZE ); \
                                                                                                               \
      if( _vl_target.data.size() == 0 )                                                                         \
      {                                                                                                        \
         _vl_target = get_default_xcall_entry( _xid );                                                           \
         KOINOS_ASSERT( _vl_target.data.size() > 0,                                                             \
            unknown_xcall,                                                                                     \
            "xcall table dispatch entry ${xid} does not exist",                                                \
            ("xid", _xid)                                                                                       \
            );                                                                                                 \
      }                                                                                                        \
                                                                                                               \
      BOOST_PP_IF(_THUNK_IS_VOID(RETURN_TYPE),,RETURN_TYPE _ret;)                                               \
                                                                                                               \
      auto _target = koinos::pack::from_vl_blob< protocol::xcall_target >( _vl_target );                         \
                                                                                                               \
      std::visit(                                                                                              \
         koinos::overloaded{                                                                                   \
            [&]( thunk_id_type& _tid ) {                                                                        \
               BOOST_PP_IF(_THUNK_IS_VOID(RETURN_TYPE),,_ret =)                                                 \
               thunk_dispatcher::instance().call_thunk<                                                        \
                  decltype(SYSCALL) >(                                                                         \
                     _tid,                                                                                      \
                     context,                                                                                  \
                     _SYSCALL_DETAIL_DEFINE_FORWARD(__VA_ARGS__) );                                            \
            },                                                                                                 \
            [&]( contract_id_type& _cid ) {                                                                     \
               /* Need xcall syscall handler */                                                                \
            },                                                                                                 \
            [&]( auto& _a ) {                                                                                   \
               KOINOS_THROW( unknown_xcall, "xcall table dispatch entry ${xid} has unimplemented type ${tag}", \
                  ("xid", _xid)("tag", _target.index()) );                                                       \
            } }, _target );                                                                                     \
      BOOST_PP_IF(_THUNK_IS_VOID(RETURN_TYPE),,return _ret;)                                                    \
   }                                                                                                           \
   RETURN_TYPE BOOST_PP_CAT(SYSCALL,_THUNK_SUFFIX)( apply_context& context, _SYSCALL_DETAIL_DEFINE_ARGS(__VA_ARGS__) )
