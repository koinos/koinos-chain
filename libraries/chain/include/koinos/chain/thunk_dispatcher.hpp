#pragma once

#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/system_calls.hpp>

#include <boost/container/flat_map.hpp>

#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>

#include <any>
#include <cstdint>
#include <functional>
#include <type_traits>

namespace koinos::chain {

namespace detail
{
   std::any get_value_from_field( const google::protobuf::Message& msg, const google::protobuf::FieldDescriptor* fd )
   {
      auto ref = msg.GetReflection();
      std::any field;

      switch( fd->cpp_type() )
      {
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_INT64:
            field = ref->GetInt64( msg, fd );
            break;
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_UINT64:
            field = ref->GetUInt64( msg, fd );
            break;
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_INT32:
            field = ref->GetInt32( msg, fd );
            break;
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_UINT32:
            field = ref->GetUInt32( msg, fd );
            break;
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_BOOL:
            field = ref->GetBool( msg, fd );
            break;
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_STRING:
            field = ref->GetString( msg, fd );
            break;
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_MESSAGE:
            field = &ref->GetMessage( msg, fd );
            break;
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_ENUM:
            field = ref->GetEnumValue( msg, fd );
            break;
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_FLOAT:
            field = ref->GetFloat( msg, fd );
            break;
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_DOUBLE:
            field = ref->GetDouble( msg, fd );
            break;
         default:
            assert( "Type not handled for thunk args." );
      }

      return field;
   }

   template< typename T, typename... Ts >
   std::enable_if_t< std::is_base_of_v< google::protobuf::Message, T >, std::tuple< T, Ts... > >
   message_to_tuple( const google::protobuf::Message& msg )
   {
      // Get our type
      constexpr std::size_t fields_remaining = sizeof...( Ts );
      auto desc = msg.GetDescriptor();
      auto fd = desc->FindFieldByNumber( desc->field_count() - fields_remaining );
      auto msg_ptr = std::any_cast< const google::protobuf::Message* >( get_value_from_field( msg, fd ) );
      T t;
      t.CopyFrom( *msg_ptr );

      if ( fields_remaining )
         return std::tuple_cat( std::tuple< T >( std::move( t ) ), message_to_tuple< Ts... >( msg ) );

      return std::tuple< T >( std::move( t ) );
   }

   template< typename T, typename... Ts >
   std::enable_if_t< !std::is_base_of_v< google::protobuf::Message, T >, std::tuple< T, Ts... > >
   message_to_tuple( const google::protobuf::Message& msg )
   {
      // Get our type
      constexpr std::size_t fields_remaining = sizeof...( Ts );
      auto desc = msg.GetDescriptor();
      auto fd = desc->FindFieldByNumber( desc->field_count() - fields_remaining );
      T t = std::any_cast< T >( get_value_from_field( msg, fd ) );

      if ( fields_remaining )
         return std::tuple_cat( std::tuple< T >( t ), message_to_tuple< Ts... >( msg ) );

      return std::tuple< T >( t );
   }

   /*
    * std::apply takes a function and a tuple and calls the function with the contents of the tuple
    * as the arguments. The only "trick" here is converting a reflected object in to an equivalent
    * tuple. That is handled as part of the reflection macro to generate the needed code. We cat a
    * tuple containing just the apply context as the first argument and then call in to the thunk.
    *
    * Two versions exist of the function, one that serializes the return value and one that does not.
    */
   template< typename ArgStruct, typename RetStruct, typename ThunkReturn, typename... ThunkArgs >
   typename std::enable_if< std::is_same< ThunkReturn, void >::value, int >::type
   call_thunk_impl( const std::function< ThunkReturn(apply_context&, ThunkArgs...) >& thunk, apply_context& ctx, char* ret_ptr, uint32_t ret_len, ArgStruct& arg )
   {
      // static_assert( std::is_same< RetStruct, std::void_t >::value, "Thunk return does not match defined return in koinos-types" );
      auto thunk_args = std::tuple_cat( std::tuple< apply_context& >( ctx ), message_to_tuple< ThunkArgs... >( arg ) );
      std::apply( thunk, thunk_args );
      return 0;
   }

   template< typename ArgStruct, typename RetStruct, typename ThunkReturn, typename... ThunkArgs >
   typename std::enable_if< !std::is_same< ThunkReturn, void >::value, int >::type
   call_thunk_impl( const std::function< ThunkReturn(apply_context&, ThunkArgs...) >& thunk, apply_context& ctx, char* ret_ptr, uint32_t ret_len, ArgStruct& arg )
   {
      static_assert( std::is_same< RetStruct, ThunkReturn >::value, "Thunk return does not match defined return in koinos-types" );
      auto thunk_args = std::tuple_cat( std::tuple< apply_context& >( ctx ), message_to_tuple< ThunkArgs... >( arg ) );
      auto ret = std::apply( thunk, thunk_args );
      std::string s;
      ret.SerializeToString( &s );
      // Maybe Koinos::Exception?
      assert( s.size() <= ret_len );
      KOINOS_TODO( "I think this copy can be optimized away possible with a string stream" );
      std::memcpy( ret_ptr, s.c_str(), s.size() );
      return 0;
   }

} // detail

/**
 * A registry for thunks.
 *
 * - A thunk is one-directional:  It's a call from WASM to native C++ code.
 * - A thunk is immutable:  A thunk always points to a particular native function.
 *
 * In particular, a semantically inequivalent upgrade to a thunk should never occur.
 * When there's a bug in a thunk implementation, the existing buggy implementation should
 * be kept under the same thunk ID.
 *
 * System governance should instead replace the system code that calls a particular
 * thunk ID with system code that calls a different thunk ID.
 *
 * When upgrading a system call from one thunk to another, the new thunk **MUST**
 * have identical function signature to the existing thunk.
 */
class thunk_dispatcher
{
   public:
      void call_thunk( uint32_t id, apply_context& ctx, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len )const;

      template< typename ThunkReturn, typename... ThunkArgs >
      auto call_thunk( uint32_t id, apply_context& ctx, ThunkArgs&... args ) const
      {
         auto it = _pass_through_map.find( id );
         KOINOS_ASSERT( it != _pass_through_map.end(), thunk_not_found, "Thunk ${id} not found", ("id", id ) );
         return std::any_cast< std::function<ThunkReturn(apply_context&, ThunkArgs...)> >(it->second)( ctx, args... );
      }

      template< typename ArgStruct, typename RetStruct, typename ThunkReturn, typename... ThunkArgs >
      void register_thunk( uint32_t id, ThunkReturn (*thunk_ptr)(apply_context&, ThunkArgs...) )
      {
         std::function<ThunkReturn(apply_context&, ThunkArgs...)> thunk = thunk_ptr;
         _dispatch_map.emplace( id, [thunk]( apply_context& ctx, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len )
         {
            memset(ret_ptr, 0, ret_len);
            ArgStruct args;
            std::string s( arg_ptr, arg_len );
            args.ParseFromString( &s );
            detail::call_thunk_impl< ArgStruct, RetStruct >( thunk, ctx, ret_ptr, ret_len, args );
         });
         _pass_through_map.emplace( id, thunk );
      }

      bool thunk_exists( uint32_t id ) const;
      static const thunk_dispatcher& instance();

   private:
      thunk_dispatcher();

      typedef std::function< void(apply_context&, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len) > generic_thunk_handler;

      boost::container::flat_map< uint32_t, generic_thunk_handler >  _dispatch_map;
      boost::container::flat_map< uint32_t, std::any >               _pass_through_map;
};

} // koinos::chain
