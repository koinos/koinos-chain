#include <koinos/chain/proto_utils.hpp>

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

} // koinos::chain
