#pragma once

#include <koinos/statedb/statedb_types.hpp>
#include <koinos/pack/rt/reflect.hpp>
#include <koinos/pack/rt/binary_serializer.hpp>

#include <mira/index_adapter.hpp>
#include <mira/ordered_index.hpp>
#include <mira/tag.hpp>
#include <mira/member.hpp>
#include <mira/indexed_by.hpp>
#include <mira/composite_key.hpp>

namespace koinos::statedb::detail {

struct state_object
{
   typedef uint64_t id_type;

   id_type           id = 0;

   object_space      space;
   object_key        key;
   object_value      value;
};

struct by_id;
struct by_key;

typedef mira::multi_index_adapter<
   state_object,
   koinos::pack::binary_serializer,
   mira::multi_index::indexed_by<
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_id >,
         mira::multi_index::member< state_object, state_object::id_type, &state_object::id > >,
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_key >,
         mira::multi_index::composite_key< state_object,
            mira::multi_index::member< state_object, object_space, &state_object::space >,
            mira::multi_index::member< state_object, object_key, &state_object::key >
         >
      >
   >
> state_object_index;

} // koinos::statedb::detail

KOINOS_REFLECT( koinos::statedb::detail::state_object,
             (id)(space)(key)(value) )
