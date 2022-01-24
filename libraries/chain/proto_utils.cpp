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

google::protobuf::Any get_field_value(
   const google::protobuf::Reflection* reflection,
   const google::protobuf::Message& message,
   const google::protobuf::FieldDescriptor* field_descriptor,
   google::protobuf::FieldDescriptor::Type type )
{
   google::protobuf::Any any;

   switch ( type )
   {
      case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
         {
            koinos::protocol::double_value d;
            d.set_value( reflection->GetDouble( message, field_descriptor ) );
            any.PackFrom( d );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_FLOAT:
         {
            koinos::protocol::float_value f;
            f.set_value( reflection->GetFloat( message, field_descriptor ) );
            any.PackFrom( f );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_INT64:
         {
            koinos::protocol::int64_value i;
            i.set_value( reflection->GetInt64( message, field_descriptor ) );
            any.PackFrom( i );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_UINT64:
         {
            koinos::protocol::uint64_value i;
            i.set_value( reflection->GetUInt64( message, field_descriptor ) );
            any.PackFrom( i );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_INT32:
         {
            koinos::protocol::int32_value i;
            i.set_value( reflection->GetInt32( message, field_descriptor ) );
            any.PackFrom( i );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_FIXED64:
         {
            koinos::protocol::fixed64_value f;
            f.set_value( reflection->GetUInt64( message, field_descriptor ) );
            any.PackFrom( f );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_FIXED32:
         {
            koinos::protocol::fixed32_value f;
            f.set_value( reflection->GetInt32( message, field_descriptor ) );
            any.PackFrom( f );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_BOOL:
         {
            koinos::protocol::bool_value b;
            b.set_value( reflection->GetBool( message, field_descriptor ) );
            any.PackFrom( b );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_STRING:
         {
            koinos::protocol::string_value s;
            s.set_value( reflection->GetString( message, field_descriptor ) );
            any.PackFrom( s );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
         {
            any.PackFrom( reflection->GetMessage( message, field_descriptor ) );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_BYTES:
         {
            koinos::protocol::bytes_value b;
            b.set_value( reflection->GetString( message, field_descriptor ) );
            any.PackFrom( b );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_UINT32:
         {
            koinos::protocol::uint32_value u;
            u.set_value( reflection->GetUInt32( message, field_descriptor ) );
            any.PackFrom( u );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_ENUM:
         {
            koinos::protocol::enum_value e;
            auto ed = reflection->GetEnum( message, field_descriptor );
            e.set_number( ed->number() );
            e.set_name( ed->name() );
            any.PackFrom( e );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
         {
            koinos::protocol::sfixed32_value s;
            s.set_value( reflection->GetInt32( message, field_descriptor ) );
            any.PackFrom( s );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
         {
            koinos::protocol::sfixed64_value s;
            s.set_value( reflection->GetInt64( message, field_descriptor ) );
            any.PackFrom( s );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_SINT32:
         {
            koinos::protocol::sint32_value s;
            s.set_value( reflection->GetInt32( message, field_descriptor ) );
            any.PackFrom( s );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_SINT64:
         {
            koinos::protocol::sint64_value s;
            s.set_value( reflection->GetInt64( message, field_descriptor ) );
            any.PackFrom( s );
         }
         break;
      default:
         KOINOS_ASSERT( false, unexpected_field_type, "attempted to retrieve the value of an unexpected field type" );
         break;
   }

   return any;
}

google::protobuf::Any get_repeated_field_value(
   const google::protobuf::Reflection* reflection,
   const google::protobuf::Message& message,
   const google::protobuf::FieldDescriptor* field_descriptor,
   google::protobuf::FieldDescriptor::Type type )
{
   google::protobuf::Any any;

   koinos::protocol::list_value list;

   int len = reflection->FieldSize( message, field_descriptor );

   switch ( type )
   {
      case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
         for ( int i = 0; i < len; i++ )
            list.add_value()->set_double_value( reflection->GetRepeatedDouble( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_FLOAT:
         for ( int i = 0; i < len; i++ )
            list.add_value()->set_float_value( reflection->GetRepeatedFloat( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_INT64:
         for ( int i = 0; i < len; i++ )
            list.add_value()->set_int64_value( reflection->GetRepeatedInt64( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_UINT64:
         for ( int i = 0; i < len; i++ )
            list.add_value()->set_uint64_value( reflection->GetRepeatedUInt64( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_INT32:
         for ( int i = 0; i < len; i++ )
            list.add_value()->set_int32_value( reflection->GetRepeatedInt32( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_FIXED64:
         for ( int i = 0; i < len; i++ )
            list.add_value()->set_fixed64_value( reflection->GetRepeatedUInt64( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_FIXED32:
         for ( int i = 0; i < len; i++ )
            list.add_value()->set_fixed32_value( reflection->GetRepeatedUInt32( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_BOOL:
         for ( int i = 0; i < len; i++ )
            list.add_value()->set_bool_value( reflection->GetRepeatedBool( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_STRING:
         for ( int i = 0; i < len; i++ )
            list.add_value()->set_string_value( reflection->GetRepeatedString( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
         for ( int i = 0; i < len; i++ )
            list.add_value()->mutable_any_value()->PackFrom( reflection->GetRepeatedMessage( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_BYTES:
         for ( int i = 0; i < len; i++ )
            list.add_value()->set_bytes_value( reflection->GetRepeatedString( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_UINT32:
         for ( int i = 0; i < len; i++ )
            list.add_value()->set_uint32_value( reflection->GetRepeatedUInt32( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_ENUM:
         for ( int i = 0; i < len; i++ )
         {
            koinos::protocol::enum_value e;
            auto ed = reflection->GetRepeatedEnum( message, field_descriptor, i );
            e.set_number( ed->number() );
            e.set_name( ed->name() );
            list.add_value()->mutable_any_value()->PackFrom( e );
         }
         break;
      case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
         for ( int i = 0; i < len; i++ )
            list.add_value()->set_sfixed32_value( reflection->GetRepeatedInt32( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
         for ( int i = 0; i < len; i++ )
            list.add_value()->set_sfixed64_value( reflection->GetRepeatedInt64( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_SINT32:
         for ( int i = 0; i < len; i++ )
            list.add_value()->set_sint32_value( reflection->GetRepeatedInt32( message, field_descriptor, i ) );
         break;
      case google::protobuf::FieldDescriptor::TYPE_SINT64:
         for ( int i = 0; i < len; i++ )
            list.add_value()->set_sint64_value( reflection->GetRepeatedInt64( message, field_descriptor, i ) );
         break;
      default:
         KOINOS_ASSERT( false, unexpected_field_type, "attempted to retrieve the value of an unexpected field type" );
         break;
   }

   any.PackFrom( list );

   return any;
}

google::protobuf::Any get_nested_field_value( execution_context& context, std::string top_level_message, std::string field_name )
{
   google::protobuf::DescriptorPool dpool;

   initialize_descriptor_pool( context, dpool );

   auto pool_descriptor = dpool.FindMessageTypeByName( top_level_message );
   KOINOS_ASSERT( pool_descriptor, unexpected_state, "protocol descriptor is null" );

   std::vector< std::string > field_path;
   boost::split( field_path, field_name, boost::is_any_of( "." ) );

   const google::protobuf::Reflection* reflection            = context.get_transaction().GetReflection();
   const google::protobuf::Message* message                  = &context.get_transaction();
   const google::protobuf::FieldDescriptor* field_descriptor = nullptr;
   google::protobuf::FieldDescriptor::Type type              = google::protobuf::FieldDescriptor::Type::MAX_TYPE;

   for ( const auto& segment : field_path )
   {
      auto pool_field_descriptor = pool_descriptor->FindFieldByName( segment );
      KOINOS_ASSERT( pool_field_descriptor, field_not_found, "unable to find field: ${fname}", ("fname", segment) );

      auto field_num             = pool_field_descriptor->number();
      type                       = pool_field_descriptor->type();
      auto message_descriptor    = message->GetDescriptor();
      field_descriptor           = message_descriptor->FindFieldByNumber( field_num );

      if ( &segment != &field_path.back() )
      {
         KOINOS_ASSERT( type == google::protobuf::FieldDescriptor::Type::TYPE_MESSAGE, unexpected_field_type, "expected nested field to be within a message" );

         message         = &reflection->GetMessage( *message, field_descriptor );
         reflection      = message->GetReflection();
         pool_descriptor = dpool.FindMessageTypeByName( message->GetTypeName() );
      }
   }

   google::protobuf::Any any;

   if ( field_descriptor->is_repeated() )
      any = get_repeated_field_value( reflection, *message, field_descriptor, type );
   else
      any = get_field_value( reflection, *message, field_descriptor, type );

   return any;
}

} // koinos::chain
