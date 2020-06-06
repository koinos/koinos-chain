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
