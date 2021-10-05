#include <koinos/chain/resources.hpp>
#include <koinos/chain/exceptions.hpp>

#include <boost/multiprecision/cpp_int.hpp>

using uint128_t = boost::multiprecision::uint128_t;

namespace koinos::chain {

/**
 * RC session
 */

rc_session::rc_session( uint64_t begin_rc ) : _begin_rc( begin_rc ), _end_rc( begin_rc ) {}
rc_session::~rc_session() = default;

void rc_session::use_rc( uint64_t rc )
{
   KOINOS_ASSERT( rc < _end_rc, insufficent_rc, "insufficent rc" );
   _end_rc -= rc;
}

uint64_t rc_session::close()
{
   return _begin_rc - _end_rc;
}

/*
 * Resource meter
 */

resource_meter::resource_meter() = default;
resource_meter::~resource_meter() = default;

void resource_meter::set_resource_limit_data( const resource_limit_data& rld )
{
   _resource_limit_data         = rld;
   _disk_storage_remaining      = _resource_limit_data.disk_storage_limit();
   _network_bandwidth_remaining = _resource_limit_data.network_bandwidth_limit();
   _compute_bandwidth_remaining = _resource_limit_data.compute_bandwidth_limit();
}

std::shared_ptr< rc_session > resource_meter::make_session( uint64_t rc )
{
   auto session = std::make_shared< rc_session >( rc );
   _session = session;
   return session;
}

void resource_meter::use_disk_storage( uint64_t bytes )
{
   KOINOS_ASSERT( bytes < _disk_storage_remaining, disk_storage_limit_exceeded, "disk storage limit exceeded" );

   if ( auto session = _session.lock() )
   {
      uint128_t rc_cost = bytes * _resource_limit_data.disk_storage_cost();
      KOINOS_ASSERT( rc_cost <= std::numeric_limits< uint64_t >::max(), rc_overflow, "rc overflow" );
      session->use_rc( rc_cost.convert_to< uint64_t >() );
   }

   _disk_storage_remaining -= bytes;
}

uint64_t resource_meter::disk_storage_used()
{
   return _resource_limit_data.disk_storage_limit() - _disk_storage_remaining;
}

void resource_meter::use_network_bandwidth( uint64_t bytes )
{
   KOINOS_ASSERT( bytes < _network_bandwidth_remaining, network_bandwidth_limit_exceeded, "network bandwidth limit exceeded" );

   if ( auto session = _session.lock() )
   {
      uint128_t rc_cost = bytes * _resource_limit_data.network_bandwidth_cost();
      KOINOS_ASSERT( rc_cost <= std::numeric_limits< uint64_t >::max(), rc_overflow, "rc overflow" );
      session->use_rc( rc_cost.convert_to< uint64_t >() );
   }

   _network_bandwidth_remaining -= bytes;
}

uint64_t resource_meter::network_bandwidth_used()
{
   return _resource_limit_data.network_bandwidth_limit() - _network_bandwidth_remaining;
}

void resource_meter::use_compute_bandwidth( uint64_t ticks )
{
   KOINOS_ASSERT( ticks < _compute_bandwidth_remaining, compute_bandwidth_limit_exceeded, "compute bandwidth limit exceeded" );

   if ( auto session = _session.lock() )
   {
      uint128_t rc_cost = ticks * _resource_limit_data.compute_bandwidth_cost();
      KOINOS_ASSERT( rc_cost <= std::numeric_limits< uint64_t >::max(), rc_overflow, "rc overflow" );
      session->use_rc( rc_cost.convert_to< uint64_t >() );
   }

   _compute_bandwidth_remaining -= ticks;
}

uint64_t resource_meter::compute_bandwidth_used()
{
   return _resource_limit_data.compute_bandwidth_limit() - _compute_bandwidth_remaining;
}

} // koinos::chain

