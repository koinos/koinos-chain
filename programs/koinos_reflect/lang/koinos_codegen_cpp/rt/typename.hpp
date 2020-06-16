#pragma once

#include <koinos/pack/rt/basetypes.hpp>

#define KOINOS_DEFINE_SIMPLE_TYPENAME( type ) \
template<> struct get_typename< type > { static const char* name() { return #type ; } };

namespace koinos::pack {

template< typename... T > struct get_typename;

namespace detail {

   template< typename... Ts >
   struct get_comma_separated_typenames;

   template<>
   struct get_comma_separated_typenames<>
   {
      static const char* names() { return ""; }
   };

   template< typename T >
   struct get_comma_separated_typenames< T >
   {
      static const char* names()
      {
         static const std::string n = get_typename< T >::name();
         return n.c_str();
      }
   };

   template< typename T, typename... Ts >
   struct get_comma_separated_typenames< T, Ts... >
   {
      static const char* names()
      {
         static const std::string n =
            std::string( get_typename<T>::name() ) + "," +
            std::string( get_comma_separated_typenames< Ts... >::names() );
         return n.c_str();
      }
   };

} // detail

using namespace koinos::protocol;

KOINOS_DEFINE_SIMPLE_TYPENAME( int8_t )
KOINOS_DEFINE_SIMPLE_TYPENAME( uint8_t )
KOINOS_DEFINE_SIMPLE_TYPENAME( int16_t )
KOINOS_DEFINE_SIMPLE_TYPENAME( uint16_t )
KOINOS_DEFINE_SIMPLE_TYPENAME( int32_t )
KOINOS_DEFINE_SIMPLE_TYPENAME( uint32_t )
KOINOS_DEFINE_SIMPLE_TYPENAME( int64_t )
KOINOS_DEFINE_SIMPLE_TYPENAME( uint64_t )
KOINOS_DEFINE_SIMPLE_TYPENAME( int128_t )
KOINOS_DEFINE_SIMPLE_TYPENAME( uint128_t )
KOINOS_DEFINE_SIMPLE_TYPENAME( int256_t )
KOINOS_DEFINE_SIMPLE_TYPENAME( uint256_t )
KOINOS_DEFINE_SIMPLE_TYPENAME( bool )
KOINOS_DEFINE_SIMPLE_TYPENAME( variable_blob )
KOINOS_DEFINE_SIMPLE_TYPENAME( multihash_type )
KOINOS_DEFINE_SIMPLE_TYPENAME( multihash_vector )

template< typename T > struct get_typename< vector< T > >
{
   static const char* name()
   {
      static std::string n = std::string( "vector<" ) + get_typename< T >::name() + ">";
      return n.c_str();
   }
};

template< typename T > struct get_typename< set< T > >
{
   static const char* name()
   {
      static std::string n = std::string( "set<" ) + get_typename< T >::name() + ">";
      return n.c_str();
   }
};

template< typename T, size_t N > struct get_typename< array< T, N > >
{
   static const char* name()
   {
      static std::string n = std::string( "array<" )
         + get_typename< T >::name() + ","
         + std::to_string( N ) + ">";
      return n.c_str();
   }
};

template< size_t N > struct get_typename< fixed_blob< N > >
{
   static const char* name()
   {
      static std::string n = std::string( "fixed_blob<" ) + std::to_string( N ) + ">";
      return n.c_str();
   }
};

template< typename... Ts > struct get_typename< variant< Ts... > >
{
   static const char* name()
   {
      static const std::string n = std::string( "variant<" )
         + detail::get_comma_separated_typenames< Ts... >::names()
         + ">";
      return n.c_str();
   }
};

template< typename T > struct get_typename< optional< T > >
{
   static const char* name()
   {
      static std::string n = std::string( "optional<" ) + get_typename< T >::name() + ">";
      return n.c_str();
   }
};

template< typename T >
inline void trim_typename_namespace( T& dest, const std::string& name )
{
   auto start = name.find_last_of( ':' );
   start = ( start == std::string::npos ) ? 0 : start + 1;
   dest = name.substr( start );
}

} // koinos::pack

#undef KOINOS_DEFINE_SIMPLE_TYPENAME
