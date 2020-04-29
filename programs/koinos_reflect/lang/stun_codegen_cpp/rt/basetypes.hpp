#pragma once

#include <boost/multiprecision/cpp_int.hpp>

#include <array>
#include <optional>
#include <set>
#include <variant>
#include <vector>

namespace koinos::protocol {

   using std::array;
   using std::optional;
   using std::set;
   using std::variant;
   using std::vector;

   typedef boost::multiprecision::int128_t  int128_t;
   typedef boost::multiprecision::uint128_t uint128_t;
   typedef boost::multiprecision::int256_t  int256_t;
   typedef boost::multiprecision::uint256_t uint256_t;

   typedef boost::multiprecision::number<
      boost::multiprecision::cpp_int_backend<
         160,
         160,
         boost::multiprecision::unsigned_magnitude,
         boost::multiprecision::unchecked, void
      >
   > uint160_t;

   typedef boost::multiprecision::number<
      boost::multiprecision::cpp_int_backend<
         160,
         160,
         boost::multiprecision::signed_magnitude,
         boost::multiprecision::unchecked, void
      >
   > int160_t;

   typedef int8_t    int8;
   typedef uint8_t   uint8;
   typedef int16_t   int16;
   typedef uint16_t  uint16;
   typedef int32_t   int32;
   typedef uint32_t  uint32;
   typedef int64_t   int64;
   typedef uint64_t  uint64;
   typedef int128_t  int128;
   typedef uint128_t uint128;
   typedef int160_t  int160;
   typedef uint160_t uint160;
   typedef int256_t  int256;
   typedef uint256_t uint256;

   struct vl_blob
   {
      vector< char > data;
   };

   template< size_t N >
   struct fl_blob
   {
      array< char, N > data;
   };

   struct multihash_type
   {
      uint64_t hash_id;
      vl_blob  digest;
   };

   struct multihash_vector
   {
      uint64_t               hash_id;
      std::vector< vl_blob > digests;
   };

} // koinos::protocol
