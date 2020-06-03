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
        protocol::to_binary(s, t);
    }

    template<typename T>
    static inline std::vector<char> to_binary_vector( const T& v )
    {
        vectorstream stream;
        protocol::to_binary(stream, v);
        return stream.vector();
    }

    template<typename Stream, typename T>
    static inline void from_binary( Stream& s, std::vector< T >& v )
    {
        protocol::from_binary(s, v);
    }
};

} // koinos::pack

