#pragma once

#include <koinos/pack/rt/basetypes.hpp>

#include <nlohmann/json.hpp>

#include <ostream>
#include <type_traits>

#define KOINOS_DECLARE_PRIMITIVE_JSON_SERIALIZER( type )      \
inline void to_json( json& j, type t );                       \
inline void from_json( json& j, type& t, uint32_t depth = 0 );\

#define KOINOS_DECLARE_BASE_JSON_SERIALIZER( type )           \
inline void to_json( json& j, const type& t );                \
inline void from_json( json& j, type& t, uint32_t depth = 0 );\

namespace koinos::pack {

using namespace koinos::types;

using json = nlohmann::json;

// Explicit bit length integers
KOINOS_DECLARE_PRIMITIVE_JSON_SERIALIZER( int8_t )
KOINOS_DECLARE_PRIMITIVE_JSON_SERIALIZER( uint8_t )
KOINOS_DECLARE_PRIMITIVE_JSON_SERIALIZER( int16_t )
KOINOS_DECLARE_PRIMITIVE_JSON_SERIALIZER( uint16_t )
KOINOS_DECLARE_PRIMITIVE_JSON_SERIALIZER( int32_t )
KOINOS_DECLARE_PRIMITIVE_JSON_SERIALIZER( uint32_t )
KOINOS_DECLARE_PRIMITIVE_JSON_SERIALIZER( int64_t )
KOINOS_DECLARE_PRIMITIVE_JSON_SERIALIZER( uint64_t )

// On gcc, size_t is identical to unsigned int. On clang, it isn't? This makes it work... /sigh
inline typename std::enable_if< !std::is_same< size_t, unsigned int >::value, void >::type
to_json( json& j, size_t v )
{
   to_json( j, (uint32_t)v );
}

inline typename std::enable_if< !std::is_same< size_t, unsigned int >::value, void >::type
from_json( json& j, size_t& v, uint32_t depth )
{
   from_json( j, (uint32_t&)v, depth );
}

KOINOS_DECLARE_BASE_JSON_SERIALIZER( int128_t )
KOINOS_DECLARE_BASE_JSON_SERIALIZER( uint128_t )
KOINOS_DECLARE_BASE_JSON_SERIALIZER( int160_t )
KOINOS_DECLARE_BASE_JSON_SERIALIZER( uint160_t )
KOINOS_DECLARE_BASE_JSON_SERIALIZER( int256_t )
KOINOS_DECLARE_BASE_JSON_SERIALIZER( uint256_t )

KOINOS_DECLARE_PRIMITIVE_JSON_SERIALIZER( bool )

KOINOS_DECLARE_BASE_JSON_SERIALIZER( multihash_type )
KOINOS_DECLARE_BASE_JSON_SERIALIZER( multihash_vector )

KOINOS_DECLARE_BASE_JSON_SERIALIZER( block_height_type )
KOINOS_DECLARE_BASE_JSON_SERIALIZER( timestamp_type )

KOINOS_DECLARE_BASE_JSON_SERIALIZER( variable_blob )
KOINOS_DECLARE_BASE_JSON_SERIALIZER( std::string )

template< size_t N >
inline void to_json( json& s, const fixed_blob< N >& v );
template< size_t N >
inline void from_json( json& s, fixed_blob< N >& v, uint32_t depth = 0 );

template< typename T >
inline void to_json( json& s, const std::vector< T >& v );
template< typename T >
inline void from_json( json& s, std::vector< T >& v, uint32_t depth = 0 );

template< typename T, size_t N >
inline void to_json( json& s, const std::array< T, N >& v );
template< typename T, size_t N >
inline void from_json( json& s, std::array< T, N >& v, uint32_t depth = 0 );

template< typename... T >
inline void to_json( json& s, const std::variant< T... >& v );
template< typename... T >
inline void from_json( json& s, std::variant< T... >& v, uint32_t depth = 0 );

template< typename T >
inline void to_json( json& s, const std::optional< T >& v );
template< typename T >
inline void from_json( json& s, std::optional< T >& v, uint32_t depth = 0 );

template< typename T >
inline void to_json( json& s, const T& v );
template< typename T >
inline void from_json( json& s, T& v, uint32_t depth = 0 );

template< typename = void, typename T = void > struct jsonifiable : std::false_type {};

template< typename T > struct jsonifiable< T, std::void_t< decltype( koinos::pack::to_json( std::declval< koinos::pack::json& >(), std::declval<T>() ) ) > > : std::true_type {};

template< typename T >
std::ostream& json_to_stream( std::ostream& o, const T& t )
{
   koinos::pack::json j;
   koinos::pack::to_json( j, t );
   o << j.dump();
   return o;
}

} // koinos::pack

#define KOINOS_DEFINE_JSON_STREAM_OPERATOR( NS )                                                    \
namespace NS {                                                                                      \
                                                                                                    \
template < typename T >                                                                             \
typename std::enable_if_t< koinos::pack::jsonifiable< T >::value, std::ostream >&                   \
operator<<( std::ostream& o, const T& t )                                                           \
{                                                                                                   \
   return koinos::pack::json_to_stream( o, t );                                                     \
}                                                                                                   \
                                                                                                    \
}

#undef KOINOS_DECLARE_PRIMITIVE_JSON_SERIALIZER
#undef KOINOS_DECLARE_BASE_JSON_SERIALIZER
