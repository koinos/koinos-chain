#pragma once

#include <fc/exception/exception.hpp>

#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>

#include <koinos/pack/rt/binary.hpp>

#include <string>

#pragma message "TODO: Delete this file after #53 is implemented"

namespace koinos { namespace statedb {

} } // koinos::statedb

namespace fc
{

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
