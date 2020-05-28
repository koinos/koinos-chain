#pragma once

#include <fc/exception/exception.hpp>

#include <chainbase/chainbase.hpp>

#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>
//#include <fc/variant.hpp>
//#include <fc/io/raw.hpp>

#include <koinos/pack/rt/binary.hpp>

#include <string>

#define KOINOS_STD_ALLOCATOR_CONSTRUCTOR( object_type )    \
   public:                                               \
      object_type () {}

namespace koinos { namespace statedb {

enum object_type
{
   state_object_type
};

} } // koinos::statedb

//namespace mira {
//template< typename T > struct is_static_length< chainbase::oid< T > > : public boost::true_type {};
//} // mira

namespace fc
{

template<typename T>
void to_variant( const chainbase::oid<T>& var, variant& vo )
{
   vo = var._id;
}

template<typename T>
void from_variant( const variant& vo, chainbase::oid<T>& var )
{
   var._id = vo.as_int64();
}

template< typename T >
struct get_typename< chainbase::oid< T > >
{
   static const char* name()
   {
      static std::string n = std::string( "chainbase::oid<" ) + get_typename< T >::name() + ">";
      return n.c_str();
   }
};

template<>
struct get_typename< koinos::protocol::multihash_type >
{
   static const char* name()
   {
      static std::string n = "koinos::protocol::multihash_type";
      return n.c_str();
   }
};

namespace raw
{

template<typename Stream, typename T>
void pack( Stream& s, const chainbase::oid<T>& id )
{
   s.write( (const char*)&id._id, sizeof(id._id) );
}

template<typename Stream, typename T>
void unpack( Stream& s, chainbase::oid<T>& id, uint32_t depth = 0 )
{
   s.read( (char*)&id._id, sizeof(id._id));
}

template< typename Stream >
void pack( Stream& s, const koinos::protocol::multihash_type& mh )
{
   koinos::pack::to_binary( s, mh );
}

template< typename Stream >
void unpack( Stream& s, koinos::protocol::multihash_type& mh, uint32_t depth = 0 )
{
   koinos::pack::from_binary( s, mh );
}

} } // fc::raw
