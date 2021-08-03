#pragma once

#include <koinos/vmmanager/api_handler.hpp>

#include <cstdint>

namespace koinos::vmmanager {

/**
 * A class containing information which may change execution of a VM.
 */
class context
{
   public:
      context( api_handler& handler, int64_t ticks )
         : _api_handler(&handler), _meter_ticks(ticks) {}

      api_handler*        _api_handler = nullptr;
      int64_t             _meter_ticks = 0;
};

} // koinos::vmmanager::context
