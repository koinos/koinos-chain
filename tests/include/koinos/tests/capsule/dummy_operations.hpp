#pragma once

#include <koinos/tests/capsule/dummy_hash.hpp>

#include <cstdint>
#include <optional>

namespace koinos::capsule {

class dummy_operations
{
   public:
      dummy_operations(int64_t size) : _size(size) {}
      virtual ~dummy_operations() {}

      std::optional< dummy_hash_ptr > get_hash( int64_t index )
      {
         std::optional< dummy_hash_ptr > result;
         if( index < _size )
            result = create_dummy_hash(index);
         return result;
      }

      dummy_hash_ptr empty_hash()
      {
         return create_dummy_hash(-1);
      }

      dummy_hash_ptr reduce(int64_t node_id, dummy_hash_ptr ha, dummy_hash_ptr hb)
      {
         return reduce_dummy_hash(ha, hb);
      }

   private:
      int64_t _size = 0;
};

}
