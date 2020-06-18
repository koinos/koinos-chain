#pragma once
#include <koinos/chain/types_fwd.hpp>

#include <koinos/chain/exceptions.hpp>

#include <koinos/exception.hpp>

#include <softfloat.hpp>

#include <memory>
#include <vector>
#include <deque>
#include <cstdint>

namespace koinos { namespace chain {

   using                               std::map;
   using                               std::vector;
   using                               std::string;
   using                               std::pair;
   using                               std::make_pair;

   struct void_t{};

} } // koinos::chain

#pragma TODO "These should be removed when we implement multi index containers on top of statedb"
namespace koinos::pack {

template<typename Stream>
void to_binary( Stream& s, const float64_t& v )
{
   to_binary(s, *reinterpret_cast<const uint64_t *>(&v));
}

template<typename Stream>
void from_binary( Stream& s, float64_t& v, uint32_t depth )
{
   from_binary(s, *reinterpret_cast<uint64_t *>(&v));
}

template<typename Stream>
void to_binary( Stream& s, const float128_t& v )
{
   to_binary(s, *reinterpret_cast<const koinos::chain::uint128_t*>(&v));
}

template<typename Stream>
void from_binary( Stream& s, float128_t& v, uint32_t depth )
{
   from_binary(s, *reinterpret_cast<koinos::chain::uint128_t*>(&v));
}

template<typename Stream>
void to_binary( Stream& s, const std::string& v )
{
   to_binary( s, unsigned_int(v.size()));
   if( v.size() ) s.write( v.c_str(), v.size() );
}

template<typename Stream>
void from_binary( Stream& s, std::string& v, uint32_t depth )
{
   depth++;
   variable_blob tmp;
   from_binary(s,tmp,depth);
   if( tmp.size() )
      v = std::string(tmp.data(),tmp.data()+tmp.size());
   else v = std::string();
}

template<typename Stream>
void to_binary( Stream& s, const koinos::chain::int128_t& v )
{
   to_binary( s, static_cast< int64_t >( v >> 64 ) );
   to_binary( s, static_cast< int64_t >( v ) );
}

template<typename Stream>
void from_binary( Stream& s, koinos::chain::int128_t& v, uint32_t depth )
{
   int64_t hi;
   int64_t lo;
   from_binary( s, hi, depth );
   from_binary( s, lo, depth );
   v = hi;
   v <<= 64;
   v |= lo;
}

template<typename Stream>
void to_binary( Stream& s, const koinos::chain::uint128_t& v )
{
   to_binary( s, static_cast< uint64_t >( v >> 64 ) );
   to_binary( s, static_cast< uint64_t >( v ) );
}

template<typename Stream>
void from_binary( Stream& s, koinos::chain::uint128_t& v, uint32_t depth )
{
   uint64_t hi;
   uint64_t lo;
   from_binary( s, hi, depth );
   from_binary( s, lo, depth );
   v = hi;
   v <<= 64;
   v |= lo;
}

} // koinos::pack
