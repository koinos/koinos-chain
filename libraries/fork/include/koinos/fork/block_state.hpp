#pragma once
#include <memory>
#include <vector>

namespace koinos::fork {

template< typename BlockType >
struct block_state
{
   using block_id_type  = decltype( BlockType::id );
   using block_num_type = decltype( BlockType::block_num );

   block_state() = delete;
   block_state( BlockType d ) : block{ std::move( d ) } {}

   block_id_type  id()          const { return block.id; }
   block_num_type block_num()   const { return block.block_num; }
   block_id_type  previous_id() const { return block.previous; }

   const BlockType block;
};

}
