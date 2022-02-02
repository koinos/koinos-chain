#include <koinos/chain/proto_utils.hpp>

#include <boost/algorithm/string.hpp>

#include <koinos/chain/state.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/protocol/value.pb.h>

namespace koinos::chain {

void initialize_descriptor_pool( execution_context& context, google::protobuf::DescriptorPool& descriptor_pool )
{
   auto pdesc = system_call::get_object( context, state::space::metadata(), state::key::protocol_descriptor );
   KOINOS_ASSERT( pdesc.exists(), unexpected_state, "file descriptor set does not exist" );

   google::protobuf::FileDescriptorSet fdesc;
   KOINOS_ASSERT( fdesc.ParseFromString( pdesc.value() ), unexpected_state, "file descriptor set is malformed" );

   for ( const auto& fd : fdesc.file() )
      descriptor_pool.BuildFile( fd );
}

value_type get_field_value(
   const google::protobuf::Reflection* reflection,
   const google::protobuf::Message& message,
   const google::protobuf::FieldDescriptor* field_descriptor,
   google::protobuf::FieldDescriptor::Type type )
{
   value_type v;

   switch ( type )
   {
      case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
         v.set_double_value( reflection->GetDouble( message, field_descriptor ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_FLOAT:
         v.set_float_value( reflection->GetFloat( message, field_descriptor ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_INT64:
         v.set_int64_value( reflection->GetInt64( message, field_descriptor ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_UINT64:
         v.set_uint64_value( reflection->GetUInt64( message, field_descriptor ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_INT32:
         v.set_int32_value( reflection->GetInt32( message, field_descriptor ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_FIXED64:
         v.set_fixed64_value( reflection->GetUInt64( message, field_descriptor ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_FIXED32:
         v.set_fixed32_value( reflection->GetInt32( message, field_descriptor ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_BOOL:
         v.set_bool_value( reflection->GetBool( message, field_descriptor ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_STRING:
         v.set_string_value( reflection->GetString( message, field_descriptor ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
         v.mutable_message_value()->PackFrom( reflection->GetMessage( message, field_descriptor ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_BYTES:
         v.set_bytes_value( reflection->GetString( message, field_descriptor ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_UINT32:
         v.set_uint32_value( reflection->GetUInt32( message, field_descriptor ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_ENUM:
         {
            koinos::chain::enum_type e;
            auto ed = reflection->GetEnum( message, field_descriptor );
            e.set_number( ed->number() );
            e.set_name( ed->name() );
            v.mutable_message_value()->PackFrom( e );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
         v.set_sfixed32_value( reflection->GetInt32( message, field_descriptor ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
         v.set_sfixed64_value( reflection->GetInt64( message, field_descriptor ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_SINT32:
         v.set_sint32_value( reflection->GetInt32( message, field_descriptor ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_SINT64:
         v.set_sint64_value( reflection->GetInt64( message, field_descriptor ) );
         break;
      default:
         KOINOS_ASSERT( false, unexpected_field_type, "attempted to retrieve the value of an unexpected field type" );
         break;
   }

   return v;
}

value_type get_repeated_field_value(
   const google::protobuf::Reflection* reflection,
   const google::protobuf::Message& message,
   const google::protobuf::FieldDescriptor* field_descriptor,
   google::protobuf::FieldDescriptor::Type type )
{
   value_type v;
   list_type list;

   int len = reflection->FieldSize( message, field_descriptor );

   switch ( type )
   {
      case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
         for ( int i = 0; i < len; i++ )
            list.add_values()->set_double_value( reflection->GetRepeatedDouble( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_FLOAT:
         for ( int i = 0; i < len; i++ )
            list.add_values()->set_float_value( reflection->GetRepeatedFloat( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_INT64:
         for ( int i = 0; i < len; i++ )
            list.add_values()->set_int64_value( reflection->GetRepeatedInt64( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_UINT64:
         for ( int i = 0; i < len; i++ )
            list.add_values()->set_uint64_value( reflection->GetRepeatedUInt64( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_INT32:
         for ( int i = 0; i < len; i++ )
            list.add_values()->set_int32_value( reflection->GetRepeatedInt32( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_FIXED64:
         for ( int i = 0; i < len; i++ )
            list.add_values()->set_fixed64_value( reflection->GetRepeatedUInt64( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_FIXED32:
         for ( int i = 0; i < len; i++ )
            list.add_values()->set_fixed32_value( reflection->GetRepeatedUInt32( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_BOOL:
         for ( int i = 0; i < len; i++ )
            list.add_values()->set_bool_value( reflection->GetRepeatedBool( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_STRING:
         for ( int i = 0; i < len; i++ )
            list.add_values()->set_string_value( reflection->GetRepeatedString( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
         for ( int i = 0; i < len; i++ )
            list.add_values()->mutable_message_value()->PackFrom( reflection->GetRepeatedMessage( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_BYTES:
         for ( int i = 0; i < len; i++ )
            list.add_values()->set_bytes_value( reflection->GetRepeatedString( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_UINT32:
         for ( int i = 0; i < len; i++ )
            list.add_values()->set_uint32_value( reflection->GetRepeatedUInt32( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_ENUM:
         for ( int i = 0; i < len; i++ )
         {
            koinos::chain::enum_type e;
            auto ed = reflection->GetRepeatedEnum( message, field_descriptor, i );
            e.set_number( ed->number() );
            e.set_name( ed->name() );
            list.add_values()->mutable_message_value()->PackFrom( e );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
         for ( int i = 0; i < len; i++ )
            list.add_values()->set_sfixed32_value( reflection->GetRepeatedInt32( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
         for ( int i = 0; i < len; i++ )
            list.add_values()->set_sfixed64_value( reflection->GetRepeatedInt64( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_SINT32:
         for ( int i = 0; i < len; i++ )
            list.add_values()->set_sint32_value( reflection->GetRepeatedInt32( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_SINT64:
         for ( int i = 0; i < len; i++ )
            list.add_values()->set_sint64_value( reflection->GetRepeatedInt64( message, field_descriptor, i ) );
         break;
      default:
         KOINOS_ASSERT( false, unexpected_field_type, "attempted to retrieve the value of an unexpected field type" );
         break;
   }

   v.mutable_message_value()->PackFrom( list );

   return v;
}

value_type get_nested_field_value( execution_context& context, const google::protobuf::Message& parent_message, std::string field_name )
{
   auto pool_descriptor = context.descriptor_pool().FindMessageTypeByName( parent_message.GetTypeName() );
   KOINOS_ASSERT( pool_descriptor, unexpected_state, "${type} descriptor was not found", ("type", parent_message.GetTypeName()) );

   std::vector< std::string > field_path;
   boost::split( field_path, field_name, boost::is_any_of( "." ) );

   const google::protobuf::Reflection* reflection            = parent_message.GetReflection();
   const google::protobuf::Message* message                  = &parent_message;
   const google::protobuf::FieldDescriptor* field_descriptor = nullptr;
   google::protobuf::FieldDescriptor::Type type              = google::protobuf::FieldDescriptor::Type::MAX_TYPE;

   for ( const auto& segment : field_path )
   {
      auto pool_field_descriptor = pool_descriptor->FindFieldByName( segment );
      KOINOS_ASSERT( pool_field_descriptor, field_not_found, "unable to find field ${fname}", ("fname", segment) );

      auto field_num          = pool_field_descriptor->number();
      type                    = pool_field_descriptor->type();
      auto message_descriptor = message->GetDescriptor();
      field_descriptor        = message_descriptor->FindFieldByNumber( field_num );
      KOINOS_ASSERT( field_descriptor, field_not_found, "unable to find field number ${fnum} on ${type} message", ("fnum", field_num)("type", message->GetTypeName()) );

      if ( &segment != &field_path.back() )
      {
         KOINOS_ASSERT( type == google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE, unexpected_field_type, "expected nested field to be within a message" );

         message         = &reflection->GetMessage( *message, field_descriptor );
         reflection      = message->GetReflection();
         pool_descriptor = context.descriptor_pool().FindMessageTypeByName( message->GetTypeName() );
      }
   }

   if ( field_descriptor->is_repeated() )
      return get_repeated_field_value( reflection, *message, field_descriptor, type );

   return get_field_value( reflection, *message, field_descriptor, type );;
}

} // koinos::chain
