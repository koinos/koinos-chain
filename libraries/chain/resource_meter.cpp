#include <koinos/chain/resource_meter.hpp>
#include <koinos/chain/session.hpp>
#include <koinos/chain/exceptions.hpp>

#include <boost/multiprecision/cpp_int.hpp>

using uint128_t = boost::multiprecision::uint128_t;

namespace koinos::chain {

/*
 * Resource meter
 */

resource_meter::resource_meter()
{
   resource_limit_data initial_rld;

   initial_rld.set_disk_storage_limit( std::numeric_limits< uint64_t >::max() );
   initial_rld.set_network_bandwidth_limit( std::numeric_limits< uint64_t >::max() );
   initial_rld.set_compute_bandwidth_limit( std::numeric_limits< uint64_t >::max() );

   set_resource_limit_data( initial_rld );
}

resource_meter::~resource_meter() = default;

void resource_meter::set_resource_limit_data( const resource_limit_data& rld )
{
   _resource_limit_data         = rld;
   _disk_storage_remaining      = _resource_limit_data.disk_storage_limit();
   _network_bandwidth_remaining = _resource_limit_data.network_bandwidth_limit();
   _compute_bandwidth_remaining = _resource_limit_data.compute_bandwidth_limit();
}

void resource_meter::use_disk_storage( uint64_t bytes )
{
   KOINOS_ASSERT( bytes <= _disk_storage_remaining, disk_storage_limit_exceeded, "disk storage limit exceeded" );

   if ( auto session = _session.lock() )
   {
      uint128_t rc_cost = uint128_t( bytes ) * _resource_limit_data.disk_storage_cost();
      KOINOS_ASSERT_REVERSION( rc_cost <= std::numeric_limits< uint64_t >::max(),  "rc overflow" );
      session->use_rc( rc_cost.convert_to< uint64_t >() );
   }

   _disk_storage_remaining -= bytes;
}

uint64_t resource_meter::disk_storage_used()
{
   return _resource_limit_data.disk_storage_limit() - _disk_storage_remaining;
}

uint64_t resource_meter::disk_storage_remaining()
{
   if ( auto session = _session.lock() )
   {
      auto cost = _resource_limit_data.disk_storage_cost();

      if ( cost > 0 )
         return session->remaining_rc() / cost;
      else
         return std::numeric_limits< uint64_t >::max();
   }

   return _disk_storage_remaining;
}

void resource_meter::use_network_bandwidth( uint64_t bytes )
{
   KOINOS_ASSERT( bytes <= _network_bandwidth_remaining, network_bandwidth_limit_exceeded, "network bandwidth limit exceeded" );

   if ( auto session = _session.lock() )
   {
      uint128_t rc_cost = uint128_t( bytes ) * _resource_limit_data.network_bandwidth_cost();
      KOINOS_ASSERT_REVERSION( rc_cost <= std::numeric_limits< uint64_t >::max(), "rc overflow" );
      session->use_rc( rc_cost.convert_to< uint64_t >() );
   }

   _network_bandwidth_remaining -= bytes;
}

uint64_t resource_meter::network_bandwidth_used()
{
   return _resource_limit_data.network_bandwidth_limit() - _network_bandwidth_remaining;
}

uint64_t resource_meter::network_bandwidth_remaining()
{
   if ( auto session = _session.lock() )
   {
      auto cost = _resource_limit_data.network_bandwidth_cost();

      if ( cost > 0 )
         return session->remaining_rc() / cost;
      else
         return std::numeric_limits< uint64_t >::max();
   }

   return _network_bandwidth_remaining;
}

void resource_meter::use_compute_bandwidth( uint64_t ticks )
{
   KOINOS_ASSERT( ticks <= _compute_bandwidth_remaining, compute_bandwidth_limit_exceeded, "compute bandwidth limit exceeded" );

   if ( auto session = _session.lock() )
   {
      uint128_t rc_cost = uint128_t( ticks ) * _resource_limit_data.compute_bandwidth_cost();
      KOINOS_ASSERT_REVERSION( rc_cost <= std::numeric_limits< uint64_t >::max(), "rc overflow" );
      session->use_rc( rc_cost.convert_to< uint64_t >() );
   }

   _compute_bandwidth_remaining -= ticks;
}

uint64_t resource_meter::compute_bandwidth_used()
{
   return _resource_limit_data.compute_bandwidth_limit() - _compute_bandwidth_remaining;
}

uint64_t resource_meter::compute_bandwidth_remaining()
{
   if ( auto session = _session.lock() )
   {
      auto cost = _resource_limit_data.compute_bandwidth_cost();

      if ( cost > 0 )
         return session->remaining_rc() / cost;
      else
         return std::numeric_limits< uint64_t >::max();
   }

   return _compute_bandwidth_remaining;
}

void resource_meter::set_session( std::shared_ptr< abstract_rc_session > s )
{
   _session = s;
}

} // koinos::chain
