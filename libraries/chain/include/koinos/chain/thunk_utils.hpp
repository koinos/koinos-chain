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
#define _THUNK_TYPE_SUFFIX _type
#define _THUNK_ARGS_SUFFIX _args

#define _THUNK_REGISTRATION( r, data, i, elem ) \
data.register_thunk<BOOST_PP_CAT(elem,_THUNK_ARGS_SUFFIX)>( thunk_id::elem, thunk::BOOST_PP_CAT(elem,_THUNK_SUFFIX) );

#define REGISTER_THUNKS( dispatcher, args )                       \
   BOOST_PP_SEQ_FOR_EACH_I( _THUNK_REGISTRATION, dispatcher, args )

#define _DEFAULT_SYS_CALL_ENTRY( SID, DATA, CALL )                \
case(system_call_id::CALL):                                       \
{                                                                 \
   retval = thunk_id::CALL;                                       \
   break;                                                         \
}

#define DEFAULT_SYSTEM_CALLS( args )                                           \
std::optional< thunk_id > get_default_system_call_entry( system_call_id sid )  \
{                                                                              \
   std::optional< thunk_id > retval;                                           \
   switch( sid )                                                               \
   {                                                                           \
      BOOST_PP_SEQ_FOR_EACH( _DEFAULT_SYS_CALL_ENTRY, sid, args )              \
      default: {} /* Do Nothing */                                             \
   }                                                                           \
   return retval;                                                              \
}

#define VA_ARGS(...) , ##__VA_ARGS__

#define THUNK_DECLARE(return_type, name, ...)                                          \
   return_type name( apply_context& VA_ARGS(__VA_ARGS__) );                            \
   return_type BOOST_PP_CAT(name,_THUNK_SUFFIX)( apply_context& VA_ARGS(__VA_ARGS__))

#define THUNK_DECLARE_VOID(return_type, name)                                          \
   return_type name( apply_context& );                                                 \
   return_type BOOST_PP_CAT(name,_THUNK_SUFFIX)( apply_context& )

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

#define _THUNK_ARG_PACK(r, blob, elem) to_variable_blob( blob, elem, true );

#define _THUNK_DETAIL_DEFINE( RETURN_TYPE, SYSCALL, ARGS, TYPES, FWD )                                               \
   RETURN_TYPE SYSCALL( apply_context& context ARGS )                                                                \
   {                                                                                                                 \
      using koinos::types::system::thunk_id_type;                                                                    \
      using koinos::types::system::contract_call_bundle;                                                             \
      using koinos::types::system::system_call_id;                                                                   \
      using koinos::types::protocol::system_call_target;                                                             \
                                                                                                                     \
      uint32_t _sid = static_cast< uint32_t >( system_call_id::SYSCALL );                                            \
                                                                                                                     \
      /* TODO Do we need to invoke serialization here? */                                                            \
      statedb::object_key _key = _sid;                                                                               \
                                                                                                                     \
      koinos::types::variable_blob _vl_target = db_get_object_thunk(                                                 \
         context, SYS_CALL_DISPATCH_TABLE_SPACE_ID, _key, SYS_CALL_DISPATCH_TABLE_OBJECT_MAX_SIZE );                 \
                                                                                                                     \
      system_call_target _target;                                                                                    \
      if( _vl_target.size() == 0 )                                                                                   \
      {                                                                                                              \
         auto maybe_thunk_id = get_default_system_call_entry( system_call_id( _sid ) );                              \
         KOINOS_ASSERT( maybe_thunk_id,                                                                              \
            unknown_system_call,                                                                                     \
            "system call table dispatch entry ${sid} does not exist",                                                \
            ("sid", _sid)                                                                                            \
            );                                                                                                       \
         _target = static_cast< thunk_id_type >( *maybe_thunk_id );                                                  \
      }                                                                                                              \
      else                                                                                                           \
      {                                                                                                              \
         _target = koinos::pack::from_variable_blob< system_call_target >( _vl_target );                             \
      }                                                                                                              \
                                                                                                                     \
      BOOST_PP_IF(_THUNK_IS_VOID(RETURN_TYPE),,RETURN_TYPE _ret;)                                                    \
                                                                                                                     \
                                                                                                                     \
      std::visit(                                                                                                    \
         koinos::overloaded{                                                                                         \
            [&]( thunk_id_type& _tid ) {                                                                             \
               BOOST_PP_IF(_THUNK_IS_VOID(RETURN_TYPE),,_ret =)                                                      \
               thunk_dispatcher::instance().call_thunk<                                                              \
                  RETURN_TYPE                                                                                        \
                  TYPES >(                                                                                           \
                     koinos::types::thunks::thunk_id(_tid),                                                          \
                     context                                                                                         \
                     FWD );                                                                                          \
            },                                                                                                       \
            [&]( contract_call_bundle& _scb ) {                                                                      \
               variable_blob _args;                                                                                  \
               BOOST_PP_LIST_FOR_EACH(_THUNK_ARG_PACK, _args, BOOST_PP_TUPLE_TO_LIST(FWD))                           \
               auto _contract_ret = thunk::execute_contract( context, _scb.contract_id, _scb.entry_point, _args );   \
               BOOST_PP_IF(_THUNK_IS_VOID(RETURN_TYPE),,koinos::pack::from_variable_blob( _contract_ret, _ret );)    \
            },                                                                                                       \
            [&]( auto& _a ) {                                                                                        \
               KOINOS_THROW( unknown_system_call,                                                                    \
                  "system call table dispatch entry ${sid} has unimplemented type ${tag}",                           \
                  ("sid", _sid)("tag", (uint64_t)_target.index()) );                                                 \
            } }, _target );                                                                                          \
      BOOST_PP_IF(_THUNK_IS_VOID(RETURN_TYPE),,return _ret;)                                                         \
   }                                                                                                                 \
   RETURN_TYPE BOOST_PP_CAT(SYSCALL,_THUNK_SUFFIX)( apply_context& context ARGS )

#define THUNK_DEFINE( RETURN_TYPE, SYSCALL, ... )                                                                    \
   _THUNK_DETAIL_DEFINE( RETURN_TYPE, SYSCALL,                                                                       \
      VA_ARGS(_THUNK_DETAIL_DEFINE_ARGS(__VA_ARGS__)),                                                               \
      VA_ARGS(_THUNK_DETAIL_DEFINE_TYPES(__VA_ARGS__)),                                                              \
      VA_ARGS(_THUNK_DETAIL_DEFINE_FORWARD(__VA_ARGS__)) )                                                           \

#define THUNK_DEFINE_VOID( RETURN_TYPE, SYSCALL )                                                                    \
   _THUNK_DETAIL_DEFINE( RETURN_TYPE, SYSCALL, , , )
