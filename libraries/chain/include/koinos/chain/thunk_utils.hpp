#pragma once
#include <boost/preprocessor.hpp>
#include <boost/preprocessor/punctuation/comma.hpp>

#include <koinos/util.hpp>

// This file exposes seven public macros for consumption
// 1. REGISTER_THUNKS
// 2. DEFAULT_SYSTEM_CALLS
// 3. THUNK_DECLARE
// 4. THUNK_DEFINE

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

#define _DEFAULT_SYS_CALL_ENTRY( sid, data, call )                \
case(BOOST_PP_CAT(call,_THUNK_ID_SUFFIX)):                        \
{                                                                 \
   target = thunk_id_type(BOOST_PP_CAT(call,_THUNK_ID_SUFFIX));   \
   koinos::pack::to_vl_blob( ret, target );                       \
   break;                                                         \
}

#define DEFAULT_SYSTEM_CALLS( args )                              \
vl_blob get_default_sys_call_entry( uint32_t sid )                \
{                                                                 \
   vl_blob ret;                                                   \
   sys_call_target target;                                        \
   switch( sid )                                                  \
   {                                                              \
      BOOST_PP_SEQ_FOR_EACH( _DEFAULT_SYS_CALL_ENTRY, sid, args ) \
      default: {} /* Do Nothing */                                \
   }                                                              \
   return ret;                                                    \
}

#define THUNK_DECLARE(return_type, name, ...)                                 \
   return_type name( apply_context&, __VA_ARGS__);                            \
   return_type BOOST_PP_CAT(name,_THUNK_SUFFIX)( apply_context&, __VA_ARGS__)

#define _THUNK_RET_TYPE_void 1)(1
#define _THUNK_IS_VOID(type) BOOST_PP_EQUAL(BOOST_PP_SEQ_SIZE((BOOST_PP_CAT(_THUNK_RET_TYPE_,type))),2)

#define _THUNK_REM(...) __VA_ARGS__
#define _THUNK_EAT(...)

// Strip off the type
#define _THUNK_STRIP(x) _THUNK_EAT x
// Show the type without parenthesis
#define _THUNK_PAIR(x) _THUNK_REM x
// Strip off the variable
#define _THUNK_TYPE(x) BOOST_PP_SEQ_ELEM(0, x)

#define _THUNK_DETAIL_DEFINE_ARGS_EACH(r, data, i, x) BOOST_PP_COMMA_IF(i) _THUNK_PAIR(x)
#define _THUNK_DETAIL_DEFINE_FORWARD_EACH(r, data, i, x) BOOST_PP_COMMA_IF(i) _THUNK_STRIP(x)
#define _THUNK_DETAIL_DEFINE_TYPES_EACH(r, data, i, x) BOOST_PP_COMMA_IF(i) _THUNK_TYPE(x)

#define _THUNK_DETAIL_DEFINE_ARGS(args) BOOST_PP_SEQ_FOR_EACH_I(_THUNK_DETAIL_DEFINE_ARGS_EACH, data, BOOST_PP_VARIADIC_TO_SEQ args)
#define _THUNK_DETAIL_DEFINE_FORWARD(args) BOOST_PP_SEQ_FOR_EACH_I(_THUNK_DETAIL_DEFINE_FORWARD_EACH, data, BOOST_PP_VARIADIC_TO_SEQ args)
#define _THUNK_DETAIL_DEFINE_TYPES(args) BOOST_PP_SEQ_FOR_EACH_I(_THUNK_DETAIL_DEFINE_TYPES_EACH, data, BOOST_PP_VARIADIC_TO_SEQ args)

#pragma message( "TODO:  Invoke smart contract sys call handler" )
#define THUNK_DEFINE( RETURN_TYPE, SYSCALL, ... )                                                                    \
   RETURN_TYPE SYSCALL( apply_context& context, _THUNK_DETAIL_DEFINE_ARGS(__VA_ARGS__) )                             \
   {                                                                                                                 \
      using koinos::protocol::thunk_id_type;                                                                         \
      using koinos::protocol::contract_id_type;                                                                      \
                                                                                                                     \
      uint32_t _sid = BOOST_PP_CAT(SYSCALL,_THUNK_ID_SUFFIX);                                                        \
                                                                                                                     \
      /* TODO Do we need to invoke serialization here? */                                                            \
      statedb::object_key _key = _sid;                                                                               \
                                                                                                                     \
      koinos::protocol::vl_blob _vl_target = db_get_object_thunk(                                                    \
         context, SYS_CALL_DISPATCH_TABLE_SPACE_ID, _key, SYS_CALL_DISPATCH_TABLE_OBJECT_MAX_SIZE );                 \
                                                                                                                     \
      if( _vl_target.data.size() == 0 )                                                                              \
      {                                                                                                              \
         _vl_target = get_default_sys_call_entry( _sid );                                                            \
         KOINOS_ASSERT( _vl_target.data.size() > 0,                                                                  \
            unknown_system_call,                                                                                     \
            "system call table dispatch entry ${sid} does not exist",                                                \
            ("sid", _sid)                                                                                            \
            );                                                                                                       \
      }                                                                                                              \
                                                                                                                     \
      BOOST_PP_IF(_THUNK_IS_VOID(RETURN_TYPE),,RETURN_TYPE _ret;)                                                    \
                                                                                                                     \
      auto _target = koinos::pack::from_vl_blob< protocol::sys_call_target >( _vl_target );                          \
                                                                                                                     \
      std::visit(                                                                                                    \
         koinos::overloaded{                                                                                         \
            [&]( thunk_id_type& _tid ) {                                                                             \
               BOOST_PP_IF(_THUNK_IS_VOID(RETURN_TYPE),,_ret =)                                                      \
               thunk_dispatcher::instance().call_thunk<                                                              \
                  RETURN_TYPE,                                                                                       \
                  _THUNK_DETAIL_DEFINE_TYPES(__VA_ARGS__) >(                                                         \
                     _tid,                                                                                           \
                     context,                                                                                        \
                     _THUNK_DETAIL_DEFINE_FORWARD(__VA_ARGS__) );                                                    \
            },                                                                                                       \
            [&]( contract_id_type& _cid ) {                                                                          \
               /* Need syscall handler */                                                                            \
            },                                                                                                       \
            [&]( auto& _a ) {                                                                                        \
               KOINOS_THROW( unknown_system_call,                                                                    \
                  "system call table dispatch entry ${sid} has unimplemented type ${tag}",                           \
                  ("sid", _sid)("tag", _target.index()) );                                                           \
            } }, _target );                                                                                          \
      BOOST_PP_IF(_THUNK_IS_VOID(RETURN_TYPE),,return _ret;)                                                         \
   }                                                                                                                 \
   RETURN_TYPE BOOST_PP_CAT(SYSCALL,_THUNK_SUFFIX)( apply_context& context, _THUNK_DETAIL_DEFINE_ARGS(__VA_ARGS__) )
