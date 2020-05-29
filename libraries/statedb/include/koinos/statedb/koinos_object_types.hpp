#pragma once

#include <fc/exception/exception.hpp>

#include <fc/reflect/variant.hpp>

#include <koinos/pack/rt/binary.hpp>
#include <koinos/pack/rt/json.hpp>

#include <string>

#pragma message "TODO: Delete this file after #53 is implemented"

namespace koinos { namespace statedb {

} } // koinos::statedb

namespace fc
{
struct variant;

void to_variant( const koinos::protocol::multihash_type& mh, variant& v )
{
   nlohmann::json j;
   koinos::pack::to_json( j, mh );
   v = j.dump();
}

void from_variant( const variant& v, koinos::protocol::multihash_type& mh )
{
   nlohmann::json j = v.as_string();
   koinos::pack::from_json( j, mh );
}

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
