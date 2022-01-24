#pragma once

#include <google/protobuf/any.pb.h>
#include <google/protobuf/message.h>
#include <google/protobuf/reflection.h>

#include <koinos/chain/execution_context.hpp>

namespace koinos::chain {

void initialize_descriptor_pool( execution_context& context, google::protobuf::DescriptorPool& descriptor_pool );

google::protobuf::Any get_field_value(
   const google::protobuf::Reflection* reflection,
   const google::protobuf::Message& message,
   const google::protobuf::FieldDescriptor* field_descriptor,
   google::protobuf::FieldDescriptor::Type type );

} // koinos::chain
