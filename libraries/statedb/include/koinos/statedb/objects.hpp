#pragma once

#include <koinos/statedb/koinos_object_types.hpp>
#include <koinos/statedb/multi_index_types.hpp>
#include <koinos/pack/rt/reflect.hpp>
#include <koinos/pack/rt/binary_serializer.hpp>

namespace koinos { namespace statedb {

class state_object : public chainbase::object< state_object_type, state_object >
{
   KOINOS_STD_ALLOCATOR_CONSTRUCTOR( state_object )

   public:
      template< typename Constructor, typename Allocator >
      state_object( Constructor&& c, Allocator&& a )
         : value( a )
      {
         c( *this );
      }

      id_type           id;

      object_space      space;
      object_key        key;
      object_value      value;
};

struct ById;
struct ByKey;

typedef multi_index_container<
   state_object,
   koinos::pack::binary_serializer,
   indexed_by<
      ordered_unique< tag< ById >, member< state_object, state_object::id_type, &state_object::id > >,
      ordered_unique< tag< ByKey >,
         composite_key< state_object,
            member< state_object, object_space, &state_object::space >,
            member< state_object, object_key, &state_object::key >
         >
      >
   >
> state_object_index;

} }

FC_REFLECT_ENUM( koinos::statedb::object_type,
   (state_object_type)
   )
KOINOS_REFLECT_ENUM( koinos::statedb::object_type,
   (state_object_type)
)

FC_REFLECT( koinos::statedb::state_object,
             (id)(space)(key)(value) )
KOINOS_REFLECT( koinos::statedb::state_object,
(id)(space)(key)(value) )

CHAINBASE_SET_INDEX_TYPE( koinos::statedb::state_object, koinos::statedb::state_object_index )
