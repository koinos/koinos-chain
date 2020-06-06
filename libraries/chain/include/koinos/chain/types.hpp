#pragma once
#include <koinos/chain/types_fwd.hpp>

#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/name.hpp>

#include <koinos/exception.hpp>

#include <softfloat.hpp>

#include <memory>
#include <vector>
#include <deque>
#include <cstdint>

#include <chainbase/chainbase.hpp>

#define OBJECT_CTOR(NAME) \
    public: \
    template<typename Constructor, typename Allocator> \
    NAME(Constructor&& c, Allocator&& a) \
    { c(*this); } \
    NAME() = default;

namespace koinos { namespace chain {

   using                               std::map;
   using                               std::vector;
   using                               std::string;
   using                               std::pair;
   using                               std::make_pair;

   struct void_t{};

   using scope_name       = name;
   using account_name     = name;
   using table_name       = name;

   /**
    * List all object types from all namespaces here so they can
    * be easily reflected and displayed in debug output.  If a 3rd party
    * wants to extend the core code then they will have to change the
    * packed_object::type field from enum_type to uint16 to avoid
    * warnings when converting packed_objects to/from json.
    *
    * UNUSED_ enums can be taken for new purposes but otherwise the offsets
    * in this enumeration are potentially shared_memory breaking
    */
   enum object_type
   {
      null_object_type = 0,
      table_id_object_type,
      key_value_object_type,
      index64_object_type,
      index128_object_type,
      index256_object_type,
      index_double_object_type,
      index_long_double_object_type,
      OBJECT_TYPE_COUNT ///< Sentry value which contains the number of different object types
   };

} } // koinos::chain

#pragma TODO "These should be removed when we implement multi index containers on top of statedb"
namespace koinos::pack {

template<typename Stream, typename T>
inline void to_binary( Stream& s, const chainbase::oid<T>& id )
{
   s.write( (const char*)&id._id, sizeof(id._id) );
}

template<typename Stream, typename T>
inline void from_binary( Stream& s, chainbase::oid<T>& id, uint32_t depth )
{
   s.read( (char*)&id._id, sizeof(id._id));
}

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
   vl_blob tmp;
   from_binary(s,tmp,depth);
   if( tmp.data.size() )
      v = std::string(tmp.data.data(),tmp.data.data()+tmp.data.size());
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

KOINOS_REFLECT_ENUM( koinos::chain::object_type,
                     (null_object_type)
                     (table_id_object_type)
                     (key_value_object_type)
                     (index64_object_type)
                     (index128_object_type)
                     (index256_object_type)
                     (index_double_object_type)
                     (index_long_double_object_type)
                     (OBJECT_TYPE_COUNT)
                   )
