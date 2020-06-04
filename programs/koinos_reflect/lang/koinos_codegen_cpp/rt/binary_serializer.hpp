#pragma once

#include <boost/interprocess/streams/vectorstream.hpp>
#include <koinos/pack/rt/binary.hpp>

#include "boost/tuple/tuple.hpp"

typedef boost::interprocess::basic_vectorstream< std::vector< char > > vectorstream;

namespace mira::multi_index{
   // fwd declaration
   template< typename T > struct composite_key_result;
}

namespace koinos::pack
{

struct binary_serializer
{
   // Types needed specifically for MIRA
   template< typename Stream, typename T >
   static inline void to_binary( Stream& s, const mira::multi_index::composite_key_result< T >& var );

   template<typename Stream, typename T>
   static inline void from_binary( Stream& s, mira::multi_index::composite_key_result< T >& var, uint32_t depth );

   template< typename Stream > static inline void to_binary( Stream&, const boost::tuples::null_type );
   template< typename Stream > static inline void from_binary( Stream& s, boost::tuples::null_type, uint32_t depth );

   template< typename Stream, typename H, typename T >
   static inline void to_binary( Stream& s, const boost::tuples::cons< H, T >& var );

   template< typename Stream, typename H, typename T >
   static inline void from_binary( Stream& s, boost::tuples::cons< H, T >& var, uint32_t depth );

   // general serializers
   template<typename Stream, typename T>
   static inline void to_binary( Stream& s, const T& t );

   template<typename Stream, typename T>
   static inline void from_binary( Stream& s, T& t, uint32_t depth );

   template<typename T>
   static inline std::vector<char> to_binary_vector( const T& v );

   template<typename T>
   static inline void from_binary_vector( const std::vector<char>& v, T& t );

   template<typename T>
   static inline T from_binary_vector( const std::vector<char>& v );

   template<typename T>
   static inline void from_binary_array( const char* data, const size_t size, T& t );

   template<typename T>
   static inline T from_binary_array( const char* data, const size_t size);

   template<typename T>
   static inline size_t binary_size( const T& v );
};

// Types needed specifically for MIRA
template< typename Stream, typename T >
inline void binary_serializer::to_binary( Stream& s, const mira::multi_index::composite_key_result< T >& var )
{
   binary_serializer::to_binary( s, var.key );
}

template<typename Stream, typename T>
inline void binary_serializer::from_binary( Stream& s, mira::multi_index::composite_key_result< T >& var, uint32_t depth )
{
   depth++;
   from_binary( s, var.key, depth );
}

template< typename Stream > inline void binary_serializer::to_binary( Stream&, const boost::tuples::null_type ) {}
template< typename Stream > inline void binary_serializer::from_binary( Stream& s, boost::tuples::null_type, uint32_t depth ) {}

template< typename Stream, typename H, typename T >
inline void binary_serializer::to_binary( Stream& s, const boost::tuples::cons< H, T >& var )
{
   binary_serializer::to_binary( s, var.get_head() );
   binary_serializer::to_binary( s, var.get_tail() );
}

template< typename Stream, typename H, typename T >
inline void binary_serializer::from_binary( Stream& s, boost::tuples::cons< H, T >& var, uint32_t depth )
{
   depth++;
   binary_serializer::from_binary( s, var.get_head(), depth );
   binary_serializer::from_binary( s, var.get_tail(), depth );
}

// general serializers
template<typename Stream, typename T>
inline void binary_serializer::to_binary( Stream& s, const T& t )
{
   koinos::pack::to_binary(s, t);
}

template<typename Stream, typename T>
inline void binary_serializer::from_binary( Stream& s, T& t, uint32_t depth )
{
   koinos::pack::from_binary(s, t, depth);
}

template<typename T>
inline std::vector<char> binary_serializer::to_binary_vector( const T& v )
{
   #pragma message "TODO: Replace with to_vl_blob"
   vectorstream stream;
   binary_serializer::to_binary(stream, v);
   return stream.vector();
}

template<typename T>
inline void binary_serializer::from_binary_vector( const std::vector<char>& v, T& t )
{
   #pragma message "TODO: Replace with from_vl_blob"
   vectorstream stream(v);
   binary_serializer::from_binary(stream, t, 0);
}

template<typename T>
inline T binary_serializer::from_binary_vector( const std::vector<char>& v )
{
   T t;
   binary_serializer::from_binary_vector(v, t);
   return t;
}

template<typename T>
inline void binary_serializer::from_binary_array( const char* data, const size_t size, T& t )
{
   #pragma message "TODO: Replace with from_vl_blob"
   std::vector<char> v(data,data+size);
   binary_serializer::from_binary_vector<T>(v, t);
}

template<typename T>
inline T binary_serializer::from_binary_array( const char* data, const size_t size)
{
   T t;
   binary_serializer::from_binary_array(data, size, t );
   return t;
}

template<typename T>
inline size_t binary_size( const T& v )
{
   #pragma message "TODO: Replace with to_vl_blob"
   vectorstream stream;
   binary_serializer::to_binary(stream, v);
   return stream.tellp();
}

} // koinos::pack

