#pragma once

#include <koinos/pack/rt/basetypes.hpp>
#include <koinos/pack/rt/varint.hpp>

#define KOINOS_DECLARE_PRIMITIVE_SERIALIZER( type )               \
template< typename Stream >                                       \
inline void to_binary( Stream& s, type t );                       \
template< typename Stream >                                       \
inline void from_binary( Stream& s, type& t, uint32_t depth = 0 );\

#define KOINOS_DECLARE_BASE_SERIALIZER( type )                    \
template< typename Stream >                                       \
inline void to_binary( Stream& s, const type& t );                \
template< typename Stream >                                       \
inline void from_binary( Stream& s, type& t, uint32_t depth = 0 );\

namespace koinos::pack {

using namespace koinos::protocol;

KOINOS_DECLARE_PRIMITIVE_SERIALIZER( int8_t )
KOINOS_DECLARE_PRIMITIVE_SERIALIZER( uint8_t )
KOINOS_DECLARE_PRIMITIVE_SERIALIZER( int16_t )
KOINOS_DECLARE_PRIMITIVE_SERIALIZER( uint16_t )
KOINOS_DECLARE_PRIMITIVE_SERIALIZER( int32_t )
KOINOS_DECLARE_PRIMITIVE_SERIALIZER( uint32_t )
KOINOS_DECLARE_PRIMITIVE_SERIALIZER( int64_t )
KOINOS_DECLARE_PRIMITIVE_SERIALIZER( uint64_t )

KOINOS_DECLARE_BASE_SERIALIZER( int128_t )
KOINOS_DECLARE_BASE_SERIALIZER( uint128_t )
KOINOS_DECLARE_BASE_SERIALIZER( int160_t )
KOINOS_DECLARE_BASE_SERIALIZER( uint160_t )
KOINOS_DECLARE_BASE_SERIALIZER( int256_t )
KOINOS_DECLARE_BASE_SERIALIZER( uint256_t )

KOINOS_DECLARE_PRIMITIVE_SERIALIZER( bool )

KOINOS_DECLARE_BASE_SERIALIZER( unsigned_int )
KOINOS_DECLARE_BASE_SERIALIZER( signed_int )

KOINOS_DECLARE_BASE_SERIALIZER( multihash_type )
KOINOS_DECLARE_BASE_SERIALIZER( multihash_vector )

KOINOS_DECLARE_BASE_SERIALIZER( vl_blob )

} // koinos::pack
