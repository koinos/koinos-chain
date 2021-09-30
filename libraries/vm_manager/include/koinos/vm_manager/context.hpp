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
      context( abstract_host_api& hapi, int64_t ticks )
         : host_api(hapi), meter_ticks(ticks) {}

      abstract_host_api& host_api;
      int64_t            meter_ticks = 0;
};

} // koinos::vm_manager::context
