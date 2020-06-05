#pragma once

#include <chainbase/allocators.hpp>
#include <chainbase/util/object_id.hpp>

#include <mira/multi_index_container_fwd.hpp>

#include <koinos/pack/rt/binary_fwd.hpp>
#include <koinos/pack/rt/json_fwd.hpp>
#include <koinos/pack/rt/string_fwd.hpp>

#include <softfloat.hpp>

#include <cstdint>

namespace koinos::chain {
   using int128_t            = __int128;
   using uint128_t           = unsigned __int128;
}

namespace fc {

inline void to_variant( const float64_t& f, variant& v );
inline void from_variant( const variant& v, float64_t& f );

inline void to_variant( const float128_t& f, variant& v );
inline void from_variant( const variant& v, float128_t& f );

namespace raw {

template<typename Stream, typename T>
inline void pack( Stream& s, const chainbase::oid<T>& id );
template<typename Stream, typename T>
inline void unpack( Stream& s, chainbase::oid<T>& id, uint32_t depth = 0 );

template<typename Stream>
void pack( Stream& s, const float64_t& v );
template<typename Stream>
void unpack( Stream& s, float64_t& v, uint32_t depth = 0 );

template<typename Stream>
void pack( Stream& s, const float128_t& v );
template<typename Stream>
void unpack( Stream& s, float128_t& v, uint32_t depth = 0 );

template<typename Stream, typename E, typename A>
void pack( Stream& s, const boost::interprocess::deque< E, A >& value );
template<typename Stream, typename E, typename A>
void unpack( Stream& s, boost::interprocess::deque< E, A >& value, uint32_t depth = 0  );

template<typename Stream, typename K, typename V, typename C, typename A>
void pack( Stream& s, const boost::interprocess::flat_map< K, V, C, A >& value );
template<typename Stream, typename K, typename V, typename C, typename A>
void unpack( Stream& s, boost::interprocess::flat_map< K, V, C, A >& value, uint32_t depth = 0  );

} } // namespace fc::raw

namespace koinos::pack {

template<typename Stream, typename T>
inline void to_binary( Stream& s, const chainbase::oid<T>& id );
template<typename Stream, typename T>
inline void from_binary( Stream& s, chainbase::oid<T>& id, uint32_t depth = 0 );

template<typename Stream>
void to_binary( Stream& s, const float64_t& v );
template<typename Stream>
void from_binary( Stream& s, float64_t& v, uint32_t depth = 0 );

template<typename Stream>
void to_binary( Stream& s, const float128_t& v );
template<typename Stream>
void from_binary( Stream& s, float128_t& v, uint32_t depth = 0 );

template<typename Stream>
void to_binary( Stream& s, const std::string& v );
template<typename Stream>
void from_binary( Stream& s, std::string& v, uint32_t depth = 0 );

template<typename Stream>
void to_binary( Stream& s, const koinos::chain::int128_t& v );
template<typename Stream>
void from_binary( Stream& s, koinos::chain::int128_t& v, uint32_t depth = 0 );

template<typename Stream>
void to_binary( Stream& s, const koinos::chain::uint128_t& v );
template<typename Stream>
void from_binary( Stream& s, koinos::chain::uint128_t& v, uint32_t depth = 0 );

}
