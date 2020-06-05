#pragma once
#include <koinos/chain/types_fwd.hpp>
#include <fc/reflect/variant.hpp>

#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/name.hpp>

#include <koinos/exception.hpp>

#include <fc/crypto/hex.hpp>
#include <fc/io/raw.hpp>

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

   using                               fc::variant;

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

namespace fc
{
class variant;

template<typename T>
void to_variant( const chainbase::oid<T>& var,  variant& vo )
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

inline
void to_variant( const float64_t& f, variant& v ) {
   v = variant(*reinterpret_cast<const double*>(&f));
}

inline
void from_variant( const variant& v, float64_t& f ) {
   from_variant(v, *reinterpret_cast<double*>(&f));
}

inline
void to_variant( const float128_t& f, variant& v ) {
   // Assumes platform is little endian and hex representation of 128-bit integer is in little endian order.
   const koinos::chain::uint128_t as_bytes = *reinterpret_cast<const koinos::chain::uint128_t*>(&f);
   std::string s = "0x";
   s.append( to_hex( reinterpret_cast<const char*>(&as_bytes), sizeof(as_bytes) ) );
   v = s;
}

inline
void from_variant( const variant& v, float128_t& f ) {
   // Temporarily hold the binary in a char* before casting to a float128_t
   char temp[16];
   auto s = v.as_string();
   FC_ASSERT( s.size() == 2 + 2 * sizeof(temp) && s.find("0x") == 0,	"Failure in converting hex data into a float128_t");
   auto sz = from_hex( s.substr(2), temp, sizeof(temp) );
   // Assumes platform is little endian and hex representation of 128-bit integer is in little endian order.
   FC_ASSERT( sz == sizeof(temp), "Failure in converting hex data into a float128_t" );
   f = *reinterpret_cast<const float128_t*>(temp);
}

namespace raw
{

template<typename Stream, typename T>
void pack( Stream& s, const chainbase::oid<T>& id )
{
   s.write( (const char*)&id._id, sizeof(id._id) );
}

template<typename Stream, typename T>
void unpack( Stream& s, chainbase::oid<T>& id, uint32_t )
{
   s.read( (char*)&id._id, sizeof(id._id));
}

template<typename Stream>
void pack( Stream& s, const float64_t& v )
{
   fc::raw::pack(s, *reinterpret_cast<const double *>(&v));
}

template<typename Stream>
void unpack( Stream& s, float64_t& v, uint32_t )
{
   fc::raw::unpack(s, *reinterpret_cast<double *>(&v));
}

template<typename Stream>
void pack( Stream& s, const float128_t& v )
{
   fc::raw::pack(s, *reinterpret_cast<const koinos::chain::uint128_t*>(&v));
}

template<typename Stream>
void unpack( Stream& s, float128_t& v, uint32_t )
{
   fc::raw::unpack(s, *reinterpret_cast<koinos::chain::uint128_t*>(&v));
}

template< typename Stream, typename E, typename A >
void pack( Stream& s, const boost::interprocess::deque<E, A>& dq )
{
   // This could be optimized
   std::vector<E> temp;
   std::copy( dq.begin(), dq.end(), std::back_inserter(temp) );
   pack( s, temp );
}

template< typename Stream, typename E, typename A >
void unpack( Stream& s, boost::interprocess::deque<E, A>& dq, uint32_t depth )
{
   depth++;
   FC_ASSERT( depth <= MAX_RECURSION_DEPTH );
   // This could be optimized
   std::vector<E> temp;
   unpack( s, temp, depth );
   dq.clear();
   std::copy( temp.begin(), temp.end(), std::back_inserter(dq) );
}

template< typename Stream, typename K, typename V, typename C, typename A >
void pack( Stream& s, const boost::interprocess::flat_map< K, V, C, A >& value )
{
   fc::raw::pack( s, unsigned_int((uint32_t)value.size()) );
   auto itr = value.begin();
   auto end = value.end();
   while( itr != end )
   {
      fc::raw::pack( s, *itr );
      ++itr;
   }
}

template< typename Stream, typename K, typename V, typename C, typename A >
void unpack( Stream& s, boost::interprocess::flat_map< K, V, C, A >& value, uint32_t depth )
{
   depth++;
   FC_ASSERT( depth <= MAX_RECURSION_DEPTH );
   unsigned_int size;
   unpack( s, size, depth );
   value.clear();
   FC_ASSERT( size.value*(sizeof(K)+sizeof(V)) < MAX_ARRAY_ALLOC_SIZE );
   for( uint32_t i = 0; i < size.value; ++i )
   {
      std::pair<K,V> tmp;
      fc::raw::unpack( s, tmp, depth );
      value.insert( std::move(tmp) );
   }
}

} } // namespace fc::raw

#pragma TODO "These should be removed when we implement multi index containers on top of statedb"
namespace koinos::pack {

template<typename Stream, typename T>
inline void to_binary( Stream& s, const chainbase::oid<T>& id )
{
   fc::raw::pack( s, id );
}

template<typename Stream, typename T>
inline void from_binary( Stream& s, chainbase::oid<T>& id, uint32_t depth )
{
   fc::raw::unpack( s, id, depth );
}

template<typename Stream>
void to_binary( Stream& s, const float64_t& v )
{
   fc::raw::pack( s, v );
}

template<typename Stream>
void from_binary( Stream& s, float64_t& v, uint32_t depth )
{
   fc::raw::unpack( s, v, depth );
}

template<typename Stream>
void to_binary( Stream& s, const float128_t& v )
{
   fc::raw::pack( s, v );
}

template<typename Stream>
void from_binary( Stream& s, float128_t& v, uint32_t depth )
{
   fc::raw::unpack( s, v, depth );
}

template<typename Stream>
void to_binary( Stream& s, const std::string& v )
{
   fc::raw::pack( s, v );
}

template<typename Stream>
void from_binary( Stream& s, std::string& v, uint32_t depth )
{
   fc::raw::unpack( s, v, depth );
}

template<typename Stream>
void to_binary( Stream& s, const koinos::chain::int128_t& v )
{
   fc::raw::pack( s, static_cast< int64_t >( v >> 64 ) );
   fc::raw::pack( s, static_cast< int64_t >( v ) );
}

template<typename Stream>
void from_binary( Stream& s, koinos::chain::int128_t& v, uint32_t depth )
{
   int64_t hi;
   int64_t lo;
   fc::raw::unpack( s, hi, depth );
   fc::raw::unpack( s, lo, depth );
   v = hi;
   v <<= 64;
   v |= lo;
}

template<typename Stream>
void to_binary( Stream& s, const koinos::chain::uint128_t& v )
{
   fc::raw::pack( s, static_cast< uint64_t >( v >> 64 ) );
   fc::raw::pack( s, static_cast< uint64_t >( v ) );
}

template<typename Stream>
void from_binary( Stream& s, koinos::chain::uint128_t& v, uint32_t depth )
{
   uint64_t hi;
   uint64_t lo;
   fc::raw::unpack( s, hi, depth );
   fc::raw::unpack( s, lo, depth );
   v = hi;
   v <<= 64;
   v |= lo;
}

} // koinos::pack

FC_REFLECT_ENUM( koinos::chain::object_type,
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
