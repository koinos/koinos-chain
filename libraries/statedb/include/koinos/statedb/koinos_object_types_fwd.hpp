#pragma once

#include <chainbase/util/object_id.hpp>

#include <koinos/pack/rt/basetypes.hpp>

#include <mira/multi_index_container_fwd.hpp>

namespace fc { namespace raw {

template< typename Stream, typename T >
void pack( Stream&, const chainbase::oid<T>& id );
template< typename Stream, typename T >
void pack( Stream&, chainbase::oid<T>& id, uint32_t depth = 0 );

template< typename Stream >
void pack( Stream&, const koinos::protocol::multihash_type& mh );
template< typename Stream >
void unpack( Stream&, koinos::protocol::multihash_type& mh, uint32_t depth = 0 );

} } // fc::raw
