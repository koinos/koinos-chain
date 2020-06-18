#pragma once
#include <boost/preprocessor.hpp>
#include <boost/preprocessor/punctuation/comma.hpp>

#include <koinos/util.hpp>

// This file exposes seven public macros for consumption
// 1. DEFINE_REGISTER_THUNKS
// 2. THUNK_DECLARE
// 3. THUNK_DEFINE

#define _THUNK_SUFFIX _thunk
#define _THUNK_ID_SUFFIX _thunk_id
#define _THUNK_TYPE_SUFFIX _type
#define _THUNK_ARGS_SUFFIX _args

#define _THUNK_REGISTRATION( r, data, i, elem ) \
td.register_thunk<BOOST_PP_CAT(elem,_THUNK_ARGS_SUFFIX)>( BOOST_PP_CAT(elem,_THUNK_ID_SUFFIX), thunk::BOOST_PP_CAT(elem,_THUNK_SUFFIX) );

#define REGISTER_THUNKS( args )                             \
void register_thunks( thunk_dispatcher& td )                \
{                                                           \
   BOOST_PP_SEQ_FOR_EACH_I( _THUNK_REGISTRATION, =>, args ) \
}

#define _DEFAULT_SYS_CALL_ENTRY( xid, data, call ) \
case(BOOST_PP_CAT(call,_THUNK_ID_SUFFIX)): \
{ \
   target = thunk_id_type(BOOST_PP_CAT(call,_THUNK_ID_SUFFIX)); \
   koinos::pack::to_vl_blob( ret, target ); \
   break; \
} \

#define DEFAULT_SYS_CALLS( args )   \
vl_blob get_default_xcall_entry( uint32_t xid ) \
{ \
   vl_blob ret; \
   xcall_target target; \
   switch( xid ) \
   { \
      BOOST_PP_SEQ_FOR_EACH( _DEFAULT_SYS_CALL_ENTRY, xid, args ) \
      default: {} /* Do Nothing */ \
   } \
   return ret; \
}

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
                  RETURN_TYPE >(                                                                           \
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
