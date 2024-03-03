#pragma once

#include <google/protobuf/any.pb.h>
#include <google/protobuf/message.h>
#include <google/protobuf/reflection.h>

#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/value.pb.h>

namespace koinos::chain {

value_type get_nested_field_value( execution_context& context, const google::protobuf::Message& parent_message, std::string field_name );

value_type get_field_value(
   const google::protobuf::Reflection* reflection,
   const google::protobuf::Message& message,
   const google::protobuf::FieldDescriptor* field_descriptor,
   google::protobuf::FieldDescriptor::Type type );

value_type get_repeated_field_value(
   const google::protobuf::Reflection* reflection,
   const google::protobuf::Message& message,
   const google::protobuf::FieldDescriptor* field_descriptor,
   google::protobuf::FieldDescriptor::Type type );

} // koinos::chain
