#pragma once
#include <koinos/chain/types_fwd.hpp>
#include <koinos/chain/types.hpp>
#include <koinos/chain/multi_index_types.hpp>

namespace koinos::chain {

class debug_state_object : public chainbase::object< debug_state_object_type, debug_state_object >
{
   OBJECT_CTOR(debug_state_object)

   id_type  id;
   uint64_t current_block; // Lower or upper 64 bits of block hash
};

using debug_state_index = multi_index_container<
   debug_state_object,
   indexed_by<
      ordered_unique< tag< by_id >,
         member< debug_state_object, debug_state_object::id_type, &debug_state_object::id >
      >
   >
>;

} // koinos::chain

FC_REFLECT( koinos::chain::debug_state_object, (id)(current_block) );
CHAINBASE_SET_INDEX_TYPE( koinos::chain::debug_state_object, koinos::chain::debug_state_index )
