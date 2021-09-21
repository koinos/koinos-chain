#pragma once

#include <cstddef>

#include <koinos/conversion.hpp>
#include <koinos/state_db/state_db_types.hpp>

#include <mira/index_adapter.hpp>
#include <mira/ordered_index.hpp>
#include <mira/tag.hpp>
#include <mira/member.hpp>
#include <mira/indexed_by.hpp>
#include <mira/composite_key.hpp>

namespace koinos::state_db::detail {

struct state_object
{
   typedef uint64_t id_type;

   id_type           id = 0;

   object_space      space;
   object_key        key;
   object_value      value;
};

struct by_id;
struct by_key;

namespace serializer::detail {

template< typename H, typename T >
inline std::vector< char > to_binary_vector( const boost::tuples::cons< H, T >& var );
template< typename H, typename T >
inline std::size_t from_binary_array( const char* data, std::size_t size, boost::tuples::cons< H, T >& var );
template< typename H, typename T >
inline std::size_t binary_size( const boost::tuples::cons< H, T >& var );

template< typename T >
inline std::vector< char > to_binary_vector( const mira::multi_index::composite_key_result< T >& var );
template< typename T >
inline std::size_t from_binary_array( const char* data, std::size_t size, mira::multi_index::composite_key_result< T >& var );
template< typename T >
inline std::size_t binary_size( const char* data, std::size_t size, mira::multi_index::composite_key_result< T >& var );

inline std::vector< char > to_binary_vector( const std::vector< std::byte >& var );
inline std::size_t from_binary_array( const char* data, std::size_t size, std::vector< std::byte >& var );
inline std::size_t binary_size( const std::vector< std::byte >& var );

inline std::vector< char > to_binary_vector( const std::vector< unsigned char >& var );
inline std::size_t from_binary_array( const char* data, std::size_t size, std::vector< unsigned char >& var );
inline std::size_t binary_size( const std::vector< unsigned char >& var );

inline std::vector< char > to_binary_vector( const crypto::multihash& var );
inline std::size_t from_binary_array( const char* data, std::size_t size, crypto::multihash& var );
inline std::size_t binary_size( const crypto::multihash& var );

inline std::vector< char > to_binary_vector( const state_object& var );
inline std::size_t from_binary_array( const char* data, std::size_t size, state_object& var );
inline std::size_t binary_size( const state_object& var );

} // detail

struct state_object_serializer {
   static inline std::vector< char > to_binary_vector( const boost::tuples::null_type )
   {
      return std::vector< char >();
   }

   static inline std::size_t from_binary_array( const char*, std::size_t, boost::tuples::null_type )
   {
      return 0;
   }

   template< typename T >
   static inline std::enable_if_t< std::is_trivially_copyable_v< T >, std::vector< char > >
   to_binary_vector( const T& v )
   {
      std::vector< char > bytes;
      bytes.resize( sizeof( T ) );
      std::copy( reinterpret_cast< const char* >( &v ),  reinterpret_cast< const char* >( &v ) + sizeof( T ), std::begin( bytes ) );
      return bytes;
   }

   template< typename T >
   static inline std::enable_if_t< std::is_trivially_copyable_v< T >, std::size_t >
   from_binary_array( const char* data, std::size_t size, T& t )
   {
      assert( sizeof( T ) <= size );
      char* obj = reinterpret_cast< char* >( std::addressof( t ) );
      std::copy( data, data + sizeof( T ), obj );
      return sizeof( T );
   }

   template< typename T >
   static inline std::enable_if_t< !std::is_trivially_copyable_v< T >, std::vector< char > >
   to_binary_vector( const T& v )
   {
      return serializer::detail::to_binary_vector( v );
   }

   template< typename T >
   static inline std::enable_if_t< !std::is_trivially_copyable_v< T >, std::size_t >
   from_binary_array( const char* data, std::size_t size, T& t )
   {
      return serializer::detail::from_binary_array( data, size, t );
   }

   template< typename T >
   static inline T from_binary_array( const char* data, std::size_t size )
   {
      T t;
      from_binary_array( data, size, t );
      return t;
   }

   template< typename T >
   static inline std::enable_if_t< std::is_trivially_copyable_v< T >, std::size_t >
   binary_size( const T& v )
   {
      return sizeof( T );
   }

   template< typename T >
   static inline std::enable_if_t< !std::is_trivially_copyable_v< T >, std::size_t >
   binary_size( const T& v )
   {
      return serializer::detail::binary_size( v );
   }
};

namespace serializer::detail {

template< typename H, typename T >
std::vector< char > to_binary_vector( const boost::tuples::cons< H, T >& var )
{
   auto a = state_object_serializer::to_binary_vector( var.get_head() );
   auto b = state_object_serializer::to_binary_vector( var.get_tail() );

   std::vector< char > ab;
   ab.reserve( a.size() + b.size() );
   ab.insert( std::end( ab ), std::begin( a ), std::end( a ) );
   ab.insert( std::end( ab ), std::begin( b ), std::end( b ) );

   return ab;
}

template< typename H, typename T >
std::size_t from_binary_array( const char* data, std::size_t size, boost::tuples::cons< H, T >& var )
{
   auto a = state_object_serializer::from_binary_array( data, size, var.get_head() );
   auto b = state_object_serializer::from_binary_array( data + a, size - a, var.get_tail() );

   return a + b;
}

template< typename H, typename T >
std::size_t binary_size( const boost::tuples::cons< H, T >& var )
{
   return state_object_serializer::binary_size( var.get_head() ) + state_object_serializer::binary_size( var.get_tail() );
}

template< typename T >
std::vector< char > to_binary_vector( const mira::multi_index::composite_key_result< T >& var )
{
   return state_object_serializer::to_binary_vector( var.key );
}

template< typename T >
std::size_t from_binary_array( const char* data, std::size_t size, mira::multi_index::composite_key_result< T >& var )
{
   return state_object_serializer::from_binary_array( data, size, var.key );
}

template< typename T >
std::size_t binary_size( const mira::multi_index::composite_key_result< T >& var )
{
   return state_object_serializer::binary_size( var.key );
}

std::vector< char > to_binary_vector( const std::vector< std::byte >& var )
{
   std::vector< char > bytes;
   bytes.resize( var.size() );
   std::transform( std::begin( var ), std::end( var ), std::begin( bytes ), []( std::byte b ) { return static_cast< char >( b ); } );
   return bytes;
}

std::size_t from_binary_array( const char* data, std::size_t size, std::vector< std::byte >& var )
{
   var.clear();
   var.resize( size );
   var.insert( std::begin( var ), reinterpret_cast< const std::byte* >( data ), reinterpret_cast< const std::byte* >( data + size ) );
   return size;
}

std::size_t binary_size( const std::vector< std::byte >& var )
{
   return var.size();
}

std::vector< char > to_binary_vector( const std::vector< unsigned char >& var )
{
   std::vector< char > bytes;
   bytes.resize( var.size() );
   std::transform( std::begin( var ), std::end( var ), std::begin( bytes ), []( unsigned char b ) { return static_cast< char >( b ); } );
   return bytes;
}

std::size_t from_binary_array( const char* data, std::size_t size, std::vector< unsigned char >& var )
{
   var.clear();
   var.resize( size );
   var.insert( std::begin( var ), reinterpret_cast< const unsigned char* >( data ), reinterpret_cast< const unsigned char* >( data + size ) );
   return size;
}

std::size_t binary_size( const std::vector< unsigned char >& var )
{
   return var.size();
}

std::vector< char > to_binary_vector( const crypto::multihash& var )
{
   return converter::as< std::vector< char > >( var );
}

std::size_t from_binary_array( const char* data, std::size_t size, crypto::multihash& var )
{
   std::vector< std::byte > bytes;
   auto n = from_binary_array( data, size, bytes );
   var = converter::to< crypto::multihash >( bytes );
   return n;
}

std::size_t binary_size( const crypto::multihash& var )
{
   return converter::as< std::vector< std::byte > >( var ).size();
}

std::vector< char > to_binary_vector( const state_object& var )
{
   auto a = state_object_serializer::to_binary_vector( var.id );
   auto b = state_object_serializer::to_binary_vector( var.space );
   auto c = state_object_serializer::to_binary_vector( var.key );
   auto d = state_object_serializer::to_binary_vector( var.value );

   a.reserve( a.size() + b.size() + c.size() + d.size() );
   a.insert( std::end( a ), std::begin( b ), std::end( b ) );
   a.insert( std::end( a ), std::begin( c ), std::end( c ) );
   a.insert( std::end( a ), std::begin( d ), std::end( d ) );

   return a;
}

std::size_t from_binary_array( const char* data, std::size_t size, state_object& var )
{
   std::size_t bytes_read = 0;

   bytes_read += state_object_serializer::from_binary_array( data, size, var.id );
   bytes_read += state_object_serializer::from_binary_array( data + bytes_read, size - bytes_read, var.space );
   bytes_read += state_object_serializer::from_binary_array( data + bytes_read, size - bytes_read, var.key );
   bytes_read += state_object_serializer::from_binary_array( data + bytes_read, size - bytes_read, var.value );

   return bytes_read;
}

std::size_t binary_size( const state_object& var )
{
   return sizeof( var.id ) + var.space.size() + var.key.size() + var.value.size();
}

} // detail

typedef mira::multi_index_adapter<
   state_object,
   state_object_serializer,
   mira::multi_index::indexed_by<
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_id >,
         mira::multi_index::member< state_object, state_object::id_type, &state_object::id > >,
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_key >,
         mira::multi_index::composite_key< state_object,
            mira::multi_index::member< state_object, object_space, &state_object::space >,
            mira::multi_index::member< state_object, object_key, &state_object::key >
         >
      >
   >
> state_object_index;

} // koinos::state_db::detail
