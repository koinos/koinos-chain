#pragma once

#include <boost/interprocess/streams/vectorstream.hpp>
#include <koinos/pack/rt/binary.hpp>

typedef boost::interprocess::basic_vectorstream< std::vector< char > > vectorstream;

namespace koinos::pack
{

struct binary_serializer
{
   template<typename Stream, typename T>
   static inline void to_binary( Stream& s, const T& t )
   {
      koinos::pack::to_binary(s, t);
   }

   template<typename T>
   static inline std::vector<char> to_binary_vector( const T& v )
   {
      vectorstream stream;
      koinos::pack::to_binary(stream, v);
      return stream.vector();
   }

   template<typename T>
   static inline T from_binary_vector( const std::vector<char>& v )
   {
      vectorstream stream(v);
      T result;
      koinos::pack::from_binary(stream, result);
      return result;
   }

   template<typename T>
   static inline T from_binary_array( const char* data, const size_t size)
   {
      std::vector<char> v(data,data+size);
      return from_binary_vector<T>(v);
   }

   template<typename Stream, typename T>
   static inline void from_binary( Stream& s, std::vector< T >& v )
   {
      koinos::pack::from_binary(s, v);
   }

   template<typename T>
   static inline size_t binary_size( const T& v )
   {
      vectorstream stream;
      koinos::pack::to_binary(stream, v);
      return stream.tellp();
   }
};

} // koinos::pack

