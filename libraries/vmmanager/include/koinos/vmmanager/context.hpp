#pragma once

#include <koinos/vmmanager/api_handler.hpp>

#include <cstdint>

namespace koinos::vmmanager {

class context
{
   public:
      context( api_handler& handler, int64_t ticks )
         : _api_handler(&handler), _meter_ticks(ticks) {}

      api_handler*        _api_handler = nullptr;
      int64_t             _meter_ticks = 0;
};

}
