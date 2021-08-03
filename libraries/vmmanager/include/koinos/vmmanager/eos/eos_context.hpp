#pragma once

#include <koinos/vmmanager/context.hpp>
#include <koinos/vmmanager/exceptions.hpp>

namespace koinos::vmmanager::eos {

class eos_context
{
   public:
      context*     _context = nullptr;

      template< typename Op >
      inline void meter( const Op& op )
      {
         int64_t cost = 1;          // TODO:  Load cost from a table instead of having it always be 1
         _context->_meter_ticks -= cost;
         if( _context->_meter_ticks < 0 )
         {
            throw tick_meter_exception{ "tick meter ran out of cycles" };
         }
      }
};

}

namespace eosio::vm {

template< typename Host, typename Op >
void meter_wasm_opcode( koinos::vmmanager::eos::eos_context* ctx, const Op& op )
{
   ctx->meter(op);
}

} // eosio::vm
