#pragma once

#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/system_calls.hpp>

#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>

#include <any>
#include <cstdint>
#include <functional>
#include <map>
#include <type_traits>

namespace koinos::chain {

namespace detail
{
   template< typename T, typename Lambda >
   std::enable_if_t< std::is_same_v< decltype( std::declval< Lambda >()( std::declval< int >() ) ), T >, void >
   iterate_repeated_field( const google::protobuf::Message& msg, const google::protobuf::FieldDescriptor* fd, std::vector< T >& v, Lambda&& l )
   {
      auto ref = msg.GetReflection();
      for( int i = 0; i < ref->FieldSize( msg, fd ); i++ )
      {
         v.emplace_back( l( i ) );
      }
   }

   template< typename T, typename Lambda >
   std::enable_if_t< !std::is_same_v< decltype( std::declval< Lambda >()( std::declval< int >() ) ), T >, void >
   iterate_repeated_field( const google::protobuf::Message& msg, const google::protobuf::FieldDescriptor* fd, std::vector< T >& v, Lambda&& l ) {}

   template< typename T >
   std::enable_if_t< std::is_base_of_v< google::protobuf::Message, T >, void >
   copy_from( const google::protobuf::Message& m, T& t )
   {
      t.CopyFrom( m );
   }

   template< typename T >
   std::enable_if_t< !std::is_base_of_v< google::protobuf::Message, T >, void >
   copy_from( const google::protobuf::Message& m, T& t ) {}

   inline std::any get_type_from_field_impl( const google::protobuf::Message& msg, const google::protobuf::FieldDescriptor* fd )
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

   // Parses through a repeated field and puts values in a vector.
   template< typename T >
   std::any get_type_from_repeated_field( const google::protobuf::Message& msg, const google::protobuf::FieldDescriptor* fd )
   {
      auto ref = msg.GetReflection();
      std::any field;

      switch( fd->cpp_type() )
      {
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_INT64:
         {
            std::vector< int64_t > v;
            iterate_repeated_field( msg, fd, v, [&]( int i )
            {
               return ref->GetRepeatedInt64( msg, fd, i );
            } );
            field = std::move( v );
            break;
         }
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_UINT64:
         {
            std::vector< uint64_t > v;
            iterate_repeated_field( msg, fd, v, [&]( int i )
            {
               return ref->GetRepeatedUInt64( msg, fd, i );
            } );
            field = std::move( v );
            break;
         }
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_INT32:
         {
            std::vector< int32_t > v;
            iterate_repeated_field( msg, fd, v, [&]( int i )
            {
               return ref->GetRepeatedInt32( msg, fd, i );
            } );
            field = std::move( v );
            break;
         }
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_UINT32:
         {
            std::vector< uint32_t > v;
            iterate_repeated_field( msg, fd, v, [&]( int i )
            {
               return ref->GetRepeatedUInt32( msg, fd, i );
            } );
            field = std::move( v );
            break;
         }
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_BOOL:
         {
            std::vector< bool > v;
            iterate_repeated_field( msg, fd, v, [&]( int i )
            {
               return ref->GetRepeatedBool( msg, fd, i );
            } );
            field = std::move( v );
            break;
         }
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_STRING:
         {
            std::vector< std::string > v;
            iterate_repeated_field( msg, fd, v, [&]( int i )
            {
               return ref->GetRepeatedString( msg, fd, i );
            } );
            field = std::move( v );
            break;
         }
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_MESSAGE:
         {
            std::vector< T > v;
            iterate_repeated_field( msg, fd, v, [&]( int i )
            {
               T t;
               copy_from( ref->GetRepeatedMessage( msg, fd, i ), t );
               return t;
            } );
            field = std::move( v );
            break;
         }
         case google::protobuf::FieldDescriptor::CppType::CPPTYPE_ENUM:
         {
            std::vector< int > v;
            iterate_repeated_field( msg, fd, v, [&]( int i )
            {
               return ref->GetRepeatedEnum( msg, fd, i );
            } );
            field = std::move( v );
            break;
         }
         default:
            assert( "Type not handled for thunk args." );
      }

      return field;
   }

   // Overload to capture when field is repeated (vector)
   template< typename T >
   void get_type_from_field( const google::protobuf::Message& msg, const google::protobuf::FieldDescriptor* fd, std::vector< T >& v )
   {
      v = std::any_cast< std::vector< T > >( get_type_from_repeated_field< T >( msg, fd ) );
   }

   // Overload to capture when field is a Message
   template< typename T >
   std::enable_if_t< std::is_base_of_v< google::protobuf::Message, T >, void >
   get_type_from_field( const google::protobuf::Message& msg, const google::protobuf::FieldDescriptor* fd, T& t )
   {
      auto ptr = std::any_cast< const google::protobuf::Message* >( get_type_from_field_impl( msg, fd ) );
      t.CopyFrom( *ptr );
   }

   // Overload to capture when field is an Enum
   template< typename T >
   std::enable_if_t< !std::is_base_of_v< google::protobuf::Message, T > && std::is_enum_v< T >, void >
   get_type_from_field( const google::protobuf::Message& msg, const google::protobuf::FieldDescriptor* fd, T& t )
   {
      t = T( std::any_cast< std::underlying_type_t< T > >( get_type_from_field_impl( msg, fd ) ) );
   }

   // Overload to capture when field is neither
   template< typename T >
   std::enable_if_t< !std::is_base_of_v< google::protobuf::Message, T > && !std::is_enum_v< T >, void >
   get_type_from_field( const google::protobuf::Message& msg, const google::protobuf::FieldDescriptor* fd, T& t )
   {
      t = std::any_cast< T >( get_type_from_field_impl( msg, fd ) );
   }

   // Variadic recusrive template to convert fields of a message in to a tuple
   // Arg type information is assumed to match the fields of the corresponding Message
   // Each call strips off the next argument (T), parses it with get_type_from_field,
   // adds it to the tuple and recusively calls with the remaining arguments (Ts).
   // The base case is when there are no more fields remaining (Ts is empty).
   template< typename T, typename... Ts >
   std::tuple< T, Ts... > message_to_tuple_impl( const google::protobuf::Message& msg )
   {
      // Get our type
      constexpr std::size_t fields_remaining = sizeof...( Ts );
      auto desc = msg.GetDescriptor();
      auto fd = desc->FindFieldByNumber( desc->field_count() - fields_remaining );
      T t;
      get_type_from_field( msg, fd, t );

      if constexpr ( fields_remaining > 0 )
         return std::tuple_cat( std::tuple< T >( std::move( t ) ), message_to_tuple_impl< Ts... >( msg ) );
      else
         return std::tuple< T >( std::move( t ) );
   }

   template< typename... Ts >
   std::tuple< std::decay_t< Ts >... > message_to_tuple( const google::protobuf::Message& msg )
   {
      return message_to_tuple_impl< std::decay_t< Ts >... >( msg );
   }

   template<>
   inline std::tuple<> message_to_tuple<>( const google::protobuf::Message& )
   {
      return std::tuple<>();
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
   typename std::enable_if< std::is_same< ThunkReturn, void >::value, uint32_t >::type
   call_thunk_impl( const std::function< ThunkReturn(execution_context&, ThunkArgs...) >& thunk, execution_context& ctx, char* ret_ptr, uint32_t ret_len, ArgStruct& arg, uint32_t* bytes_written  )
   {
      auto thunk_args = std::tuple_cat( std::tuple< execution_context& >( ctx ), message_to_tuple< ThunkArgs... >( arg ) );
      int32_t code = 0;
      *bytes_written = 0;

      try
      {
         std::apply( thunk, thunk_args );
      }
      catch ( const chain_reversion& )
      {
         code = constants::chain_reversion;
      }
      catch ( const chain_failure& )
      {
         code = constants::chain_failure;
      }

      return code;
   }

   template< typename ArgStruct, typename RetStruct, typename ThunkReturn, typename... ThunkArgs >
   typename std::enable_if< !std::is_same< ThunkReturn, void >::value, uint32_t >::type
   call_thunk_impl( const std::function< ThunkReturn(execution_context&, ThunkArgs...) >& thunk, execution_context& ctx, char* ret_ptr, uint32_t ret_len, ArgStruct& arg, uint32_t* bytes_written )
   {
      static_assert( std::is_same< RetStruct, ThunkReturn >::value, "thunk return does not match defined return in koinos-proto" );
      int32_t code = 0;
      auto thunk_args = std::tuple_cat( std::tuple< execution_context& >( ctx ), message_to_tuple< ThunkArgs... >( arg ) );
      ThunkReturn ret;

      try
      {
         ret = std::apply( thunk, thunk_args );
      }
      catch ( const chain_reversion& )
      {
         code = constants::chain_reversion;
      }
      catch ( const chain_failure& )
      {
         code = constants::chain_failure;
      }

      std::string s;
      ret.SerializeToString( &s );
      KOINOS_ASSERT( s.size() <= ret_len, insufficient_return_buffer_exception, "return buffer is not large enough for the return value" );
      #pragma message "TODO: We should avoid making copies where possible (Issue #473)"
      std::memcpy( ret_ptr, s.c_str(), s.size() );
      *bytes_written = s.size();

      return code;
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
      int32_t call_thunk( uint32_t id, execution_context& ctx, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len, uint32_t* bytes_written  )const;

      template< typename ThunkReturn, typename... ThunkArgs >
      auto call_thunk( uint32_t id, execution_context& ctx, ThunkArgs&... args ) const
      {
         auto it = _pass_through_map.find( id );
         KOINOS_ASSERT( it != _pass_through_map.end(), unknown_thunk_exception, "thunk ${id} not found", ("id", id ) );
         return std::any_cast< std::function<ThunkReturn(execution_context&, ThunkArgs...)> >(it->second)( ctx, args... );
      }

      template< typename ArgStruct, typename RetStruct, typename ThunkReturn, typename... ThunkArgs >
      void register_thunk( uint32_t id, ThunkReturn (*thunk_ptr)(execution_context&, ThunkArgs...) )
      {
         std::function<ThunkReturn(execution_context&, ThunkArgs...)> thunk = thunk_ptr;
         _dispatch_map.insert_or_assign( id, [thunk]( execution_context& ctx, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len, uint32_t* bytes_written ) -> uint32_t
         {
            ArgStruct args;
            ctx.resource_meter().use_compute_bandwidth( ctx.get_compute_bandwidth( "deserialize_message_per_byte" ) * arg_len );
            std::string s( arg_ptr, arg_len );
            args.ParseFromString( s );
            return detail::call_thunk_impl< ArgStruct, RetStruct >( thunk, ctx, ret_ptr, ret_len, args, bytes_written );
         });
         _pass_through_map.insert_or_assign( id, thunk );
      }

      template< typename ArgStruct, typename RetStruct, typename ThunkReturn, typename... ThunkArgs >
      void register_genesis_thunk( uint32_t id, ThunkReturn (*thunk_ptr)(execution_context&, ThunkArgs...) )
      {
         register_thunk< ArgStruct, RetStruct, ThunkReturn, ThunkArgs... >( id, thunk_ptr );
         _genesis_thunks.insert( id );
      }

      bool thunk_exists( uint32_t id ) const;
      bool thunk_is_genesis( uint32_t ) const;
      static const thunk_dispatcher& instance();

   private:
      thunk_dispatcher();

      typedef std::function< int32_t(execution_context&, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len, uint32_t* bytes_written) > generic_thunk_handler;

      std::map< int32_t, generic_thunk_handler > _dispatch_map;
      std::map< int32_t, std::any >              _pass_through_map;
      std::set< uint32_t >                       _genesis_thunks;
};

} // koinos::chain
