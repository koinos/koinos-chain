#pragma once

#include <koinos/vm_manager/host_api.hpp>

#include <cstdint>

namespace koinos::vm_manager {

/**
 * A class containing information which may change the execution of a VM.
 */
class context
{
   public:
      context() = delete;
      context( host_api& hapi, int64_t ticks )
         : _host_api(hapi), _meter_ticks(ticks) {}

      host_api&           _host_api;
      int64_t             _meter_ticks = 0;
};

} // koinos::vm_manager::context
