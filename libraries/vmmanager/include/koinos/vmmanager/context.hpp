#pragma once

#include <koinos/vmmanager/chain_host_api.hpp>

#include <cstdint>

namespace koinos::vmmanager {

class context
{
   public:
      context( chain_host_api& hapi, int64_t ticks )
         : _chain_host_api(&hapi), _meter_ticks(ticks) {}

      chain_host_api*     _chain_host_api = nullptr;
      int64_t             _meter_ticks = 0;
};

}
