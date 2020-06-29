#pragma once

#include <cstddef>

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/serialization/strong_typedef.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <set>
#include <variant>
#include <vector>

namespace koinos::types {

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

   typedef bool      boolean;
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

   using variable_blob = std::vector< char >;

   template < size_t N >
   using fixed_blob    = std::array< char, N >;

   BOOST_STRONG_TYPEDEF( uint64_t, timestamp_type );
   BOOST_STRONG_TYPEDEF( uint64_t, block_height_type );

   struct multihash_type
   {
      uint64_t       hash_id = 0;
      variable_blob  digest;
   };

   struct multihash_vector
   {
      uint64_t                     hash_id = 0;
      std::vector< variable_blob > digests;
   };

} // koinos::types

// thunk_id and syscall_id should be moved to be language agnostic

namespace koinos::types::thunks {

enum class thunk_id : uint32_t
{
   prints,
   verify_block_header,
   apply_block,
   apply_transaction,
   apply_reserved_operation,
   apply_upload_contract_operation,
   apply_execute_contract_operation,
   apply_set_system_call_operation,
   db_put_object,
   db_get_object,
   db_get_next_object,
   db_get_prev_object,
   execute_contract,
   get_contract_args_size,
   get_contract_args
};

} // koinos::types::thunks

namespace koinos::types::system {

enum class system_call_id : uint32_t
{
   prints,
   verify_block_header,
   apply_block,
   apply_transaction,
   apply_reserved_operation,
   apply_upload_contract_operation,
   apply_execute_contract_operation,
   apply_set_system_call_operation,
   db_put_object,
   db_get_object,
   db_get_next_object,
   db_get_prev_object,
   execute_contract,
   get_contract_args_size,
   get_contract_args
};

} // koinos::types::system
