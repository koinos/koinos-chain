#pragma once
#include <boost/preprocessor.hpp>
#include <boost/preprocessor/punctuation/comma.hpp>
#include <boost/vmd/is_empty.hpp>

#include <koinos/crypto/merkle_tree.hpp>

#include <koinos/chain/system_call_ids.pb.h>

#include <koinos/util/conversion.hpp>

#include <type_traits>

// This file exposes seven public macros for consumption
// 1. SYSTEM_CALL_DEFAULTS
// 2. THUNK_REGISTER
// 3. THUNK_DECLARE
// 4. THUNK_DEFINE

#define _THUNK_TYPE_SUFFIX _type
#define _THUNK_ARGS_SUFFIX _arguments
#define _THUNK_RET_SUFFIX  _result

#define _THUNK_REGISTRATION_GENESIS( r, data, i, elem ) \
data.register_genesis_thunk<BOOST_PP_CAT(elem,_THUNK_ARGS_SUFFIX),BOOST_PP_CAT(elem,_THUNK_RET_SUFFIX)>( chain::system_call_id::elem, thunk::BOOST_PP_CAT(_, elem) );

#define THUNK_REGISTER_GENESIS( dispatcher, args ) \
   BOOST_PP_SEQ_FOR_EACH_I( _THUNK_REGISTRATION_GENESIS, dispatcher, args )

#define _THUNK_REGISTRATION( r, data, i, elem ) \
data.register_thunk<BOOST_PP_CAT(elem,_THUNK_ARGS_SUFFIX),BOOST_PP_CAT(elem,_THUNK_RET_SUFFIX)>( chain::system_call_id::elem, thunk::BOOST_PP_CAT(_, elem) );

#define THUNK_REGISTER( dispatcher, args ) \
   BOOST_PP_SEQ_FOR_EACH_I( _THUNK_REGISTRATION, dispatcher, args )

#define VA_ARGS(...) , ##__VA_ARGS__

#define THUNK_DECLARE(return_type, name, ...)                                                        \
   namespace thunk { return_type BOOST_PP_CAT(_, name)( execution_context& VA_ARGS(__VA_ARGS__) ); } \
   namespace system_call {                                                                           \
   BOOST_PP_IF(                                                                                      \
      _THUNK_IS_VOID(return_type),                                                                   \
      void,                                                                                          \
      std::remove_reference_t< decltype( std::declval< return_type >().value() ) >                   \
   ) name( execution_context& VA_ARGS(__VA_ARGS__) ); }

#define THUNK_DECLARE_VOID(return_type, name)                                      \
   namespace thunk { return_type BOOST_PP_CAT(_, name)( execution_context& ); }    \
   namespace system_call {                                                         \
   BOOST_PP_IF(                                                                    \
      _THUNK_IS_VOID(return_type),                                                 \
      void,                                                                        \
      std::remove_reference_t< decltype( std::declval<return_type>().value() ) >   \
   ) name( execution_context& ); }

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

namespace koinos::chain::detail {
   inline void set_message_field( google::protobuf::Message& msg, std::size_t index, int64_t value )
   {
      auto desc = msg.GetDescriptor();
      auto ref = msg.GetReflection();
      auto fd = desc->FindFieldByNumber( index );
      assert( fd );
      if ( fd->type() == google::protobuf::FieldDescriptor::Type::TYPE_ENUM )
         ref->SetEnumValue( &msg, fd, (int)value );
      else
         ref->SetInt64( &msg, fd, value );
   }

   inline void set_message_field( google::protobuf::Message& msg, std::size_t index, uint64_t value )
   {
      auto desc = msg.GetDescriptor();
      auto ref = msg.GetReflection();
      auto fd = desc->FindFieldByNumber( index );
      assert( fd );
      ref->SetUInt64( &msg, fd, value );
   }

   inline void set_message_field( google::protobuf::Message& msg, std::size_t index, int32_t value )
   {
      auto desc = msg.GetDescriptor();
      auto ref = msg.GetReflection();
      auto fd = desc->FindFieldByNumber( index );
      assert( fd );
      if ( fd->type() == google::protobuf::FieldDescriptor::Type::TYPE_ENUM )
         ref->SetEnumValue( &msg, fd, (int)value );
      else
         ref->SetInt32( &msg, fd, value );
   }

   inline void set_message_field( google::protobuf::Message& msg, std::size_t index, uint32_t value )
   {
      auto desc = msg.GetDescriptor();
      auto ref = msg.GetReflection();
      auto fd = desc->FindFieldByNumber( index );
      assert( fd );
      ref->SetUInt32( &msg, fd, value );
   }

   inline void set_message_field( google::protobuf::Message& msg, std::size_t index, bool value )
   {
      auto desc = msg.GetDescriptor();
      auto ref = msg.GetReflection();
      auto fd = desc->FindFieldByNumber( index );
      assert( fd );
      ref->SetBool( &msg, fd, value );
   }

   inline void set_message_field( google::protobuf::Message& msg, std::size_t index, const std::string& value )
   {
      auto desc = msg.GetDescriptor();
      auto ref = msg.GetReflection();
      auto fd = desc->FindFieldByNumber( index );
      assert( fd );
      ref->SetString( &msg, fd, value );
   }

   inline void set_message_field( google::protobuf::Message& msg, std::size_t index, const google::protobuf::Message& value )
   {
      auto desc = msg.GetDescriptor();
      auto ref = msg.GetReflection();
      auto fd = desc->FindFieldByNumber( index );
      assert( fd );
      auto m = ref->MutableMessage( &msg, fd );
      m->CopyFrom( value );
   }

   inline void set_message_field( google::protobuf::Message& msg, std::size_t index, const std::vector< uint64_t >& values )
   {
      auto desc = msg.GetDescriptor();
      auto ref = msg.GetReflection();
      auto fd = desc->FindFieldByNumber( index );
      assert( fd );
      assert( fd->is_repeated() );
      for( const auto& v : values )
         ref->AddUInt64( &msg, fd, v );
   }

   inline void set_message_field( google::protobuf::Message& msg, std::size_t index, const std::vector< int64_t >& values )
   {
      auto desc = msg.GetDescriptor();
      auto ref = msg.GetReflection();
      auto fd = desc->FindFieldByNumber( index );
      assert( fd );
      assert( fd->is_repeated() );
      for( const auto& v : values )
         ref->AddInt64( &msg, fd, v );
   }

   inline void set_message_field( google::protobuf::Message& msg, std::size_t index, const std::vector< uint32_t >& values )
   {
      auto desc = msg.GetDescriptor();
      auto ref = msg.GetReflection();
      auto fd = desc->FindFieldByNumber( index );
      assert( fd );
      assert( fd->is_repeated() );
      for( const auto& v : values )
         ref->AddUInt32( &msg, fd, v );
   }

   inline void set_message_field( google::protobuf::Message& msg, std::size_t index, const std::vector< int32_t >& values )
   {
      auto desc = msg.GetDescriptor();
      auto ref = msg.GetReflection();
      auto fd = desc->FindFieldByNumber( index );
      assert( fd );
      assert( fd->is_repeated() );
      for( const auto& v : values )
      {
         if ( fd->type() == google::protobuf::FieldDescriptor::Type::TYPE_ENUM )
            ref->AddEnumValue( &msg, fd, (int)v );
         else
            ref->AddInt32( &msg, fd, v );
      }
   }

   inline void set_message_field( google::protobuf::Message& msg, std::size_t index, const std::vector< bool >& values )
   {
      auto desc = msg.GetDescriptor();
      auto ref = msg.GetReflection();
      auto fd = desc->FindFieldByNumber( index );
      assert( fd );
      assert( fd->is_repeated() );
      for( const auto& v : values )
         ref->AddBool( &msg, fd, v );
   }

   inline void set_message_field( google::protobuf::Message& msg, std::size_t index, const std::vector< std::string >& values )
   {
      auto desc = msg.GetDescriptor();
      auto ref = msg.GetReflection();
      auto fd = desc->FindFieldByNumber( index );
      assert( fd );
      assert( fd->is_repeated() );
      for( const auto& v : values )
         ref->AddString( &msg, fd, v );
   }

   template< typename T >
   std::enable_if_t< std::is_base_of_v< google::protobuf::Message, T >, void >
   inline set_message_field( google::protobuf::Message& msg, std::size_t index, const std::vector< T >& values )
   {
      auto desc = msg.GetDescriptor();
      auto ref = msg.GetReflection();
      auto fd = desc->FindFieldByNumber( index );
      assert( fd );
      assert( fd->is_repeated() );
      for( const auto& v : values )
      {
         auto m = ref->AddMessage( &msg, fd );
         m->CopyFrom( v );
      }
   }
}

#define _THUNK_DETAIL_ARG_PACK(r, msg, i, elem) koinos::chain::detail::set_message_field( msg, i + 1, elem );
#define _THUNK_ARG_PACK( FIRST, ... ) BOOST_PP_LIST_FOR_EACH_I(_THUNK_DETAIL_ARG_PACK, _args, BOOST_PP_TUPLE_TO_LIST((__VA_ARGS__)))

#define _THUNK_DETAIL_DEFINE( RETURN_TYPE, SYSCALL, ARGS, TYPES, FWD )                                                     \
   }                                                                                                                       \
   namespace system_call {                                                                                                 \
   auto SYSCALL( execution_context& context ARGS ) ->                                                                      \
      BOOST_PP_IF(                                                                                                         \
         _THUNK_IS_VOID(RETURN_TYPE),                                                                                      \
         void,                                                                                                             \
         std::remove_reference_t< decltype( std::declval<RETURN_TYPE>().value() ) >                                        \
      )                                                                                                                    \
   {                                                                                                                       \
                                                                                                                           \
      uint32_t _sid = static_cast< uint32_t >( chain::system_call_id::SYSCALL );                                           \
                                                                                                                           \
      BOOST_PP_IF(_THUNK_IS_VOID(RETURN_TYPE),,RETURN_TYPE _ret;)                                                          \
                                                                                                                           \
      with_stack_frame (                                                                                                   \
         context,                                                                                                          \
         stack_frame {                                                                                                     \
            .sid = _sid,                                                                                                   \
            .call_privilege = privilege::kernel_mode                                                                       \
         },                                                                                                                \
         [&]() {                                                                                                           \
            if ( context.system_call_exists( _sid ) )                                                                      \
            {                                                                                                              \
               BOOST_PP_CAT( SYSCALL, _THUNK_ARGS_SUFFIX ) _args;                                                          \
               BOOST_PP_IF(BOOST_VMD_IS_EMPTY(FWD),,_THUNK_ARG_PACK(FWD));                                                 \
               std::string _arg_str;                                                                                       \
               _args.SerializeToString( &_arg_str );                                                                       \
               auto [ _ret_str, code ] = context.system_call( _sid, _arg_str );                                            \
               if ( code >= chain::reversion )                                                                             \
                  throw chain::reversion_exception( code, _ret_str );                                                      \
               else if ( code <= chain::failure )                                                                          \
                  throw chain::failure_exception( code, _ret_str );                                                        \
               else if ( _sid == system_call_id::exit )                                                                    \
                  throw chain::success_exception( code );                                                                  \
               BOOST_PP_IF(_THUNK_IS_VOID(RETURN_TYPE),,_ret.ParseFromString( _ret_str );)                                 \
            }                                                                                                              \
            else                                                                                                           \
            {                                                                                                              \
               auto _thunk_id = context.thunk_translation( _sid );                                                         \
               auto _desc = chain::system_call_id_descriptor();                                                            \
               auto _enum_value = _desc->FindValueByNumber( _thunk_id );                                                   \
               KOINOS_ASSERT( _enum_value, unknown_thunk_exception, "unrecognized thunk id ${id}", ("id", _thunk_id) );                   \
               auto _compute = context.get_compute_bandwidth( _enum_value->name() );                                       \
               context.resource_meter().use_compute_bandwidth( _compute );                                                 \
               BOOST_PP_IF(_THUNK_IS_VOID(RETURN_TYPE),,_ret =)                                                            \
                  thunk_dispatcher::instance().call_thunk<                                                                 \
                     RETURN_TYPE                                                                                           \
                     TYPES >(                                                                                              \
                        _thunk_id,                                                                                         \
                        context                                                                                            \
                        FWD );                                                                                             \
            }                                                                                                              \
         }                                                                                                                 \
      );                                                                                                                   \
                                                                                                                           \
      BOOST_PP_IF(_THUNK_IS_VOID(RETURN_TYPE),,return _ret.value();)                                                       \
   }                                                                                                                       \
   }                                                                                                                       \
   namespace thunk {                                                                                                       \
   RETURN_TYPE BOOST_PP_CAT(_, SYSCALL)( execution_context& context ARGS )

#define THUNK_DEFINE( RETURN_TYPE, SYSCALL, ... )                                                                    \
   _THUNK_DETAIL_DEFINE( RETURN_TYPE, SYSCALL,                                                                       \
      VA_ARGS(_THUNK_DETAIL_DEFINE_ARGS(__VA_ARGS__)),                                                               \
      VA_ARGS(_THUNK_DETAIL_DEFINE_TYPES(__VA_ARGS__)),                                                              \
      VA_ARGS(_THUNK_DETAIL_DEFINE_FORWARD(__VA_ARGS__)))                                                            \

#define THUNK_DEFINE_VOID( RETURN_TYPE, SYSCALL )                                                                    \
   _THUNK_DETAIL_DEFINE( RETURN_TYPE, SYSCALL, , , )

#define THUNK_DEFINE_BEGIN() namespace thunk {
#define THUNK_DEFINE_END()   }
