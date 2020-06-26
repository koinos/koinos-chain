#pragma once
#include <koinos/pack/rt/json_fwd.hpp>

#include <koinos/pack/rt/exceptions.hpp>
#include <koinos/pack/rt/reflect.hpp>
#include <koinos/pack/rt/typename.hpp>
#include <koinos/pack/rt/util/base58.hpp>
#include <koinos/pack/rt/util/variant_helpers.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/mpl/size.hpp>

#define JSON_MAX_SAFE_INTEGER ((1ll<<53)-1)  // 2^53-1
#define JSON_MIN_SAFE_INTEGER (-((1ll<<53)-1)) // -(2^53-1)

#define JSON_SIGNED_INT_SERIALIZER( int_type )                                               \
inline void to_json( json& j, int_type v )                                                   \
{                                                                                            \
   j = int64_t( v );                                                                         \
}                                                                                            \
inline void from_json( json& j, int_type& v, uint32_t depth )                                \
{                                                                                            \
   int64_t tmp = j.template get< int64_t >();                                                \
   if( !(tmp <= static_cast< int64_t >( std::numeric_limits< int_type >::max() )             \
      && tmp >= static_cast< int64_t >( std::numeric_limits< int_type >::min() )) )          \
      throw json_int_out_of_bounds( "Over/underflow when parsing " #int_type " from JSON" ); \
   v = static_cast< int_type >( tmp );                                                       \
}

#define JSON_UNSIGNED_INT_SERIALIZER( int_type )                                             \
inline void to_json( json& j, int_type v )                                                   \
{                                                                                            \
   j = uint64_t( v );                                                                        \
}                                                                                            \
inline void from_json( json& j, int_type& v, uint32_t depth )                                \
{                                                                                            \
   uint64_t tmp = j.template get< uint64_t >();                                              \
   if( !(tmp <= static_cast< uint64_t >( std::numeric_limits< int_type >::max())) )          \
      throw json_int_out_of_bounds( "Over/underflow when parsing " #int_type " from JSON" ); \
   v = static_cast< int_type >( tmp );                                                       \
}

#define JSON_SIGNED_BOOST_INT_SERIALIZER( int_type )              \
inline void to_json( json& j, const int_type& v )                 \
{                                                                 \
   if( v > JSON_MAX_SAFE_INTEGER || v < JSON_MIN_SAFE_INTEGER )   \
   {                                                              \
      j = v.str();                                                \
   }                                                              \
   else                                                           \
   {                                                              \
      j = v.template convert_to< int64_t >();                     \
   }                                                              \
}                                                                 \
inline void from_json( json& j, int_type& v, uint32_t depth )     \
{                                                                 \
   if( j.is_string() )                                            \
   {                                                              \
      v = int_type( j.template get< string >() );                 \
   }                                                              \
   else                                                           \
   {                                                              \
      v = j.template get< int64_t >();                            \
   }                                                              \
}

#define JSON_UNSIGNED_BOOST_INT_SERIALIZER( int_type )            \
inline void to_json( json& j, const int_type& v )                 \
{                                                                 \
   if( v > JSON_MAX_SAFE_INTEGER )                                \
   {                                                              \
      j = v.str();                                                \
   }                                                              \
   else                                                           \
   {                                                              \
      j = v.template convert_to< uint64_t >();                    \
   }                                                              \
}                                                                 \
inline void from_json( json& j, int_type& v, uint32_t depth )     \
{                                                                 \
   if( j.is_string() )                                            \
   {                                                              \
      v = int_type( j.template get< string >() );                 \
   }                                                              \
   else                                                           \
   {                                                              \
      v = j.template get< uint64_t >();                           \
   }                                                              \
}

#define JSON_BOOST_STRONG_TYPEDEF_SERIALIZER( type )      \
inline void to_json( json& j, const type& v )             \
{                                                         \
   to_json( j, v.t );                                     \
}                                                         \
inline void from_json( json& j, type& v, uint32_t depth ) \
{                                                         \
   from_json( j, v.t, depth );                            \
}

namespace koinos::pack
{

using std::string;

// int types

JSON_SIGNED_INT_SERIALIZER( int8_t )
JSON_UNSIGNED_INT_SERIALIZER( uint8_t )
JSON_SIGNED_INT_SERIALIZER( int16_t )
JSON_UNSIGNED_INT_SERIALIZER( uint16_t )
JSON_SIGNED_INT_SERIALIZER( int32_t )
JSON_UNSIGNED_INT_SERIALIZER( uint32_t )

JSON_SIGNED_INT_SERIALIZER( long )
JSON_UNSIGNED_INT_SERIALIZER( unsigned long )

JSON_BOOST_STRONG_TYPEDEF_SERIALIZER( block_height_type )
JSON_BOOST_STRONG_TYPEDEF_SERIALIZER( timestamp_type )

inline void to_json( json& j, int64_t v )
{
   if( v > JSON_MAX_SAFE_INTEGER || v < JSON_MIN_SAFE_INTEGER )
   {
      j = std::to_string( v );
   }
   else
   {
      j = v;
   }
}

inline void from_json( json& j, int64_t& v, uint32_t depth )
{
   if( j.is_string() )
   {
      v = boost::lexical_cast< int64_t >( j.template get< string >() );
   }
   else
   {
      v = j.template get< int64_t >();
   }
}

inline void to_json( json& j, uint64_t v )
{
   if( v > JSON_MAX_SAFE_INTEGER )
   {
      j = std::to_string( v );
   }
   else
   {
      j = v;
   }
}

inline void from_json( json& j, uint64_t& v, uint32_t depth )
{
   if( j.is_string() )
   {
      v = boost::lexical_cast< uint64_t >( j.template get< string >() );
   }
   else
   {
      v = j.template get< uint64_t >();
   }
}

JSON_SIGNED_BOOST_INT_SERIALIZER( int128_t )
JSON_UNSIGNED_BOOST_INT_SERIALIZER( uint128_t )
JSON_SIGNED_BOOST_INT_SERIALIZER( int160_t )
JSON_UNSIGNED_BOOST_INT_SERIALIZER( uint160_t )
JSON_SIGNED_BOOST_INT_SERIALIZER( int256_t )
JSON_UNSIGNED_BOOST_INT_SERIALIZER( uint256_t )

// bool

inline void to_json( json& j, bool v ) { j = v; }
inline void from_json( json& j, bool& v, uint32_t depth ) { v = j.template get< bool >(); }

// vector< T >
template< typename T >
inline void to_json( json& j, const vector< T >& v )
{
   for( const T& t : v )
   {
      json tmp;
      to_json( tmp, t );
      j.emplace_back( std::move( tmp ) );
   }
}

template< typename T >
inline void from_json( json& j, vector< T >& v, uint32_t depth )
{
   depth++;
   if( !(depth <= KOINOS_PACK_MAX_RECURSION_DEPTH) ) throw depth_violation( "Unpack depth exceeded" );
   if( !(j.is_array()) ) throw json_type_mismatch( "Unexpected JSON type: Array Expected" );
   v.clear();
   for( json& obj : j )
   {
      T tmp;
      from_json( obj, tmp, depth );
      v.emplace_back( std::move( tmp ) );
   }
}

// variable length blob
inline void to_json( json& j, const variable_blob& v )
{
   string base58;
   util::encode_base58( base58, v );
   j = std::move( 'z' + base58 );
}

inline void from_json( json& j, variable_blob& v, uint32_t depth )
{
   if( !(j.is_string()) ) throw json_type_mismatch( "Unexpected JSON type: String Expected" );
   v.clear();
   string encoded_str = j.template get< string >();
   switch( encoded_str.c_str()[0] )
   {
      case 'm':
         // Base64
         break;
      case 'z':
         // Base58
         if( !(util::decode_base58( encoded_str.c_str() + 1, v )) ) throw json_decode_error( "Error decoding base58 string" );
         break;
      default:
         if( !(false) ) throw json_type_mismatch( "Unknown encoding prefix" );
   }
}

inline void to_json( json& j, const std::string& s )
{
   j = s;
}

inline void from_json( json& j, std::string& s )
{
   if( !(j.is_string()) ) throw json_type_mismatch( "Unexpected JSON type: String Expected" );
   s = j.template get< string >();
}

// set< T >
template< typename T >
inline void to_json( json& j, const set< T >& v )
{
   for( const T& t : v )
   {
      json tmp;
      to_json( tmp, t );
      j.emplace_back( std::move( tmp ) );
   }
}

template< typename T >
inline void from_json( json& j, set< T >& v, uint32_t depth )
{
   depth++;
   if( !(depth <= KOINOS_PACK_MAX_RECURSION_DEPTH) ) throw depth_violation( "Unpack depth exceeded" );
   if( !(j.is_array()) ) throw json_type_mismatch( "Unexpected JSON type: Array Expected" );
   v.clear();
   for( json& obj : j )
   {
      T tmp;
      from_json( obj, tmp, depth );
      auto res = v.emplace( std::move( tmp ) );
      if( !(res.second) ) throw parse_error( "Duplicate value detected deserializing set" );
   }
}

// array< T, N >
template< typename T, size_t N >
inline void to_json( json& j, const array< T, N >& v )
{
   for( const T& t : v )
   {
      json tmp;
      to_json( tmp, t );
      j.emplace_back( std::move( tmp ) );
   }
}

template< typename T, size_t N >
inline void from_json( json& j, array< T, N >& v, uint32_t depth )
{
   depth++;
   if( !(depth <= KOINOS_PACK_MAX_RECURSION_DEPTH) ) throw depth_violation( "Unpack depth exceeded" );
   if( !(j.is_array()) ) throw json_type_mismatch( "Unexpected JSON type: Array Expected" );
   if( !(j.size() == N) ) throw json_type_mismatch( "JSON array is incorrect size" );
   for( size_t i = 0; i < N; ++i )
   {
      from_json( j[i], v[i], depth );
   }
}

// fixed length blob
template< size_t N >
inline void to_json( json& j, const fixed_blob< N >& v )
{
   string base58;
   util::encode_base58( base58, v );
   j = std::move( 'z' + base58 );
}

template< size_t N >
inline void from_json( json& j, fixed_blob< N >& v, uint32_t depth )
{
   if( !(j.is_string()) ) throw json_type_mismatch( "Unexpected JSON type: String Expected" );

   string encoded_str = j.template get< string >();
   switch( encoded_str.c_str()[0] )
   {
      case 'm':
         // Base64
         break;
      case 'z':
         // Base58
         if( !(util::decode_base58( encoded_str.c_str() + 1, v )) ) throw json_decode_error( "Error decoding base58 string" );
         break;
      default:
         if( !(false) ) throw json_type_mismatch( "Unknown encoding prefix" );
   }
}

// variant
template< typename... Ts >
inline void to_json( json& j, const variant< Ts... >& v )
{
   std::visit( [&]( auto& arg ){
      using variant_type = std::decay_t< decltype(arg) >;
      trim_typename_namespace( j[ "type" ], get_typename< variant_type >::name() );
      to_json( j[ "value" ], arg );
   }, v );
}

template< typename... Ts >
inline void from_json( json& j, variant< Ts... >& v, uint32_t depth )
{
   static std::map< string, int64_t > to_tag = []()
   {
      std::map< string, int64_t > name_map;
      for( size_t i = 0; i < sizeof...(Ts); ++i )
      {
         string n;
         util::variant_helper< Ts... >::get_typename_at( i, n );
         name_map[n] = i;
      }
      return name_map;
   }();

   depth++;
   if( !(depth <= KOINOS_PACK_MAX_RECURSION_DEPTH) ) throw depth_violation( "Unpack depth exceeded" );
   if( !(j.is_object()) ) throw json_type_mismatch( "Unexpected JSON type: object expected" );
   if( !(j.size() == 2) ) throw json_type_mismatch( "Variant JSON type must only contain two fields" );
   if( !(j.contains( "type" )) ) throw json_type_mismatch( "Variant JSON type must contain field 'type'" );
   if( !(j.contains( "value" )) ) throw json_type_mismatch( "Variant JSON type must contain field 'value'" );

   auto type = j[ "type" ];
   int64_t index = -1;

   if( type.is_number_integer() )
   {
      index = type.get< int64_t >();
   }
   else if( type.is_string() )
   {
      auto itr = to_tag.find( type.get< string >() );
      if( !(itr != to_tag.end()) ) throw json_type_mismatch( "Invalid type name in JSON variant" );
      index = itr->second;
   }
   else
   {
      if( !(false) ) throw json_type_mismatch( "Variant JSON 'type' must be an unsigned integer or string" );
   }

   util::variant_helper< Ts... >::init_variant( v, index );
   std::visit( [&]( auto& arg ){ from_json( j[ "value" ], arg, depth ); }, v );
}

// optional
template< typename T >
inline void to_json( json& j, const optional< T >& v )
{
   if( v.has_value() ) to_json( j, *v );
}

template< typename T >
inline void from_json( json& j, optional< T >& v, uint32_t depth )
{
   depth++;
   if( !(depth <= KOINOS_PACK_MAX_RECURSION_DEPTH) ) throw depth_violation( "Unpack depth exceeded" );
   if( j.is_null() )
   {
      v.reset();
   }
   else
   {
      T tmp;
      from_json( j, tmp, depth );
      v = std::move( tmp );
   }
}

// multihash
inline void to_json( json& j, const multihash_type& v )
{
   json tmp;
   to_json( tmp, v.digest );
   j[ "hash" ] = v.hash_id;
   j[ "digest" ] = std::move( tmp );
}

inline void from_json( json& j, multihash_type& v, uint32_t depth )
{
   if( !(j.is_object()) ) throw json_type_mismatch( "Unexpected JSON type: object exptected" );
   if( !(j.size() == 2) ) throw json_type_mismatch( "Multihash JSON type must only contain two fields" );
   if( !(j.contains( "hash" )) ) throw json_type_mismatch( "Multihash JSON type must contain field 'hash'" );
   if( !(j.contains( "digest" )) ) throw json_type_mismatch( "Multihash JSON type must contain field 'digest'" );

   v.hash_id = j[ "hash" ].get< uint64_t >();
   from_json( j[ "digest" ], v.digest );
}

// multihash vector
inline void to_json( json& j, const multihash_vector& v )
{
   j[ "hash" ] = v.hash_id;
      for( const auto& d : v.digests )
   {
      json tmp;
      to_json( tmp, d );
      j[ "digests" ].emplace_back( std::move( tmp ) );
   }
}

inline void from_json( json& j, multihash_vector& v, uint32_t depth )
{
   if( !(j.is_object()) ) throw json_type_mismatch( "Unexpected JSON type: object exptected" );
   if( !(j.size() == 2) ) throw json_type_mismatch( "MultihashVector JSON type must only contain two fields" );
   if( !(j.contains( "hash" )) ) throw json_type_mismatch( "MultihashVector JSON type must contain field 'hash'" );
   if( !(j.contains( "digests" )) ) throw json_type_mismatch( "MultihashVector JSON type must contain field 'digests'" );

   json& digests = j[ "digests" ];

   if( !(digests.is_array()) ) throw json_type_mismatch( "MultihashVector field 'digest' must be an array" );
   for( json& d : digests )
   {
      variable_blob tmp;
      from_json( d, tmp );
      v.digests.emplace_back( std::move( tmp ) );
   }

   v.hash_id = j[ "hash" ].get< uint64_t >();
}

namespace detail::json {

   template< typename T > std::true_type is_class_helper( void(T::*)() );
   template< typename T > std::false_type is_class_helper( ... );

   template<typename T>
   struct is_class { typedef decltype( is_class_helper<T>(0) ) type; enum value_enum { value = type::value }; };

   template< typename Json, typename Class >
   struct to_json_object_visitor
   {
      to_json_object_visitor( const Class& _c, Json& _j ) :
         c(_c),
         j(_j)
      {}

      template< typename T, typename C, T(C::*p) >
      void operator()( const char* name )const
      {
         to_json( j[name], c.*p );
      }

      private:
         const Class& c;
         Json&        j;
   };

   template< typename Json, typename Class >
   struct from_json_object_visitor
   {
      from_json_object_visitor( Class& _c, Json& _j ) :
         c(_c),
         j(_j)
      {}

      template< typename T, typename C, T(C::*p) >
      inline void operator()( const char* name )const
      {
         auto itr = j.find( name );
         if( itr != j.end() )
            from_json( *itr, c.*p );
      }

      private:
         Class& c;
         Json&  j;
   };

   // if_class is only called if type is not reflected
   template< typename IsClass = std::true_type >
   struct if_class
   {
      template< typename Json, typename T >
      static inline void to_json( Json& j, const T& v ) { j = v; }
      template< typename Json, typename T >
      static inline void from_json( Json& j, T& v, uint32_t depth ) { v = j.template get< T >(); }
   };

   template<>
   struct if_class< std::false_type >
   {
      template< typename Json, typename T >
      static inline void to_json( Json& j, const T& v )
      {
         j = v;
      }

      template< typename Json, typename T >
      static inline void from_json( Json& j, T& v, uint32_t )
      {
         v = j.template get< T >();
      }
   };

   // if_enum is only called if type is reflected
   template< typename IsEnum = std::false_type >
   struct if_enum
   {
      template< typename Json, typename T >
      static inline void to_json( Json& j, const T& v )
      {
         reflector< T >::visit( to_json_object_visitor< Json, T >( v, j ) );
      }

      template< typename Json, typename T >
      static inline void from_json( Json& j, T& v, uint32_t depth )
      {
         reflector< T >::visit( from_json_object_visitor< Json, T >( v, j ) );
      }
   };

   template<>
   struct if_enum< std::true_type >
   {
      template< typename Json, typename T >
      static inline void to_json( Json& j, const T& v )
      {
         to_json( j, (int64_t)v );
      }

      template< typename Json, typename T >
      static inline void from_json( Json& j, T& v, uint32_t depth )
      {
         depth++;
         if( !(depth <= KOINOS_PACK_MAX_RECURSION_DEPTH) ) throw depth_violation( "Unpack depth exceeded" );
         int64_t temp;
         from_json(j, temp, depth);
         v = (T)temp;
      }
   };

} // detail

template< typename T >
void to_json( json& j, const T& v )
{
   detail::json::if_enum< typename reflector< T >::is_enum >::to_json( j, v );
}

template< typename T >
void from_json( json& j, T& v, uint32_t depth )
{
   detail::json::if_enum< typename reflector< T >::is_enum >::from_json( j, v, depth );
}

} // koinos::json

#undef JSON_MAX_SAFE_INTEGER
#undef JSON_MIN_SAFE_INTEGER
#undef JSON_SIGNED_INT_SERIALIZER
#undef JSON_UNSIGNED_INT_SERIALIZER
#undef JSON_SIGNED_BOOST_INT_SERIALIZER
#undef JSON_UNSIGNED_BOOST_INT_SERIALIZER
