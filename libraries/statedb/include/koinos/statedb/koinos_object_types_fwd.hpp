#pragma once

#include <koinos/pack/rt/basetypes.hpp>

#include <mira/multi_index_container_fwd.hpp>

namespace fc { namespace raw {

template< typename Stream >
void pack( Stream&, const koinos::protocol::multihash_type& mh );
template< typename Stream >
void unpack( Stream&, koinos::protocol::multihash_type& mh, uint32_t depth = 0 );

} } // fc::raw
