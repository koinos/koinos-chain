#pragma once

#include <koinos/chain/chain.pb.h>

#include <memory>

namespace koinos::chain {

namespace compute_load {
   constexpr uint64_t light  = 100;
   constexpr uint64_t medium = 1000;
   constexpr uint64_t heavy  = 10000;
}

struct abstract_rc_session
{
   virtual void use_rc( uint64_t rc ) = 0;
   virtual uint64_t remaining_rc() = 0;
   virtual uint64_t used_rc() = 0;
};

class resource_meter final
{
public:
   resource_meter();
   ~resource_meter();

   void set_resource_limit_data( const resource_limit_data& rld );

   void set_session( std::shared_ptr< abstract_rc_session > s );

   void use_disk_storage( uint64_t bytes );
   uint64_t disk_storage_used() const;
   uint64_t disk_storage_remaining() const;
   uint64_t system_disk_storage_used() const;

   void use_network_bandwidth( uint64_t bytes );
   uint64_t network_bandwidth_used() const;
   uint64_t network_bandwidth_remaining() const;
   uint64_t system_network_bandwidth_used() const;

   void use_compute_bandwidth( uint64_t ticks );
   uint64_t compute_bandwidth_used() const;
   uint64_t compute_bandwidth_remaining() const;
   uint64_t system_compute_bandwidth_used() const;

private:

   uint64_t _disk_storage_remaining        = 0;
   uint64_t _system_disk_storage_used      = 0;
   uint64_t _network_bandwidth_remaining   = 0;
   uint64_t _system_network_bandwidth_used = 0;
   uint64_t _compute_bandwidth_remaining   = 0;
   uint64_t _system_compute_bandwidth_used = 0;
   resource_limit_data _resource_limit_data;
   std::weak_ptr< abstract_rc_session > _session;
};

} // koinos::chain
