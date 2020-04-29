#pragma once

#include <koinos/statedb/koinos_object_types.hpp>
#include <koinos/statedb/multi_index_types.hpp>

namespace koinos { namespace statedb {

class StateObject : public chainbase::object< StateObjectType, StateObject >
{
   KOINOS_STD_ALLOCATOR_CONSTRUCTOR( StateObject )

   public:
      template< typename Constructor, typename Allocator >
      StateObject( Constructor&& c, Allocator&& a )
         : value( a )
      {
         c( *this );
      }

      id_type           id;

      ObjectSpace       space;
      ObjectKey         key;
      ObjectValue       value;
};

struct ById;
struct ByKey;

typedef multi_index_container<
   StateObject,
   indexed_by<
      ordered_unique< tag< ById >, member< StateObject, StateObject::id_type, &StateObject::id > >,
      ordered_unique< tag< ByKey >,
         composite_key< StateObject,
            member< StateObject, ObjectSpace, &StateObject::space >,
            member< StateObject, ObjectKey, &StateObject::key >
         >
      >
   >
> StateObjectIndex;

} }

FC_REFLECT_ENUM( koinos::statedb::ObjectType,
   (StateObjectType)
   )
FC_REFLECT( koinos::statedb::StateObject,
             (id)(space)(key)(value) )
CHAINBASE_SET_INDEX_TYPE( koinos::statedb::StateObject, koinos::statedb::StateObjectIndex )
