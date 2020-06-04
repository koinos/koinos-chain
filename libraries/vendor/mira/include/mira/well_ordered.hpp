#pragma once
#include <boost/tuple/tuple.hpp>

#include <mira/detail/slice_compare.hpp>

#include <fc/time.hpp>
#include <fc/uint128.hpp>
#include <fc/crypto/sha256.hpp>

#define MIRA_WELL_ORDERED_TYPE( t ) \
template< typename Serializer, bool ROOT > struct is_well_ordered< t, Serializer, ROOT > : public boost::true_type {};

namespace mira {

   using multi_index::composite_key_compare;
   using multi_index::detail::slice_comparator;

   /*
      Partial composite keys will lookup in rocksdb by filling in the missing parts with
      default initialized values. We are defining a type well-ordered if the default value
      is the lowest value.
    */
   template< typename T, typename Serializer, bool ROOT > struct is_well_ordered
   {
      static const bool value = ROOT;
   };

   template< typename T, typename Serializer, bool ROOT > struct is_well_ordered< std::less< T >, Serializer, ROOT > : is_well_ordered< T, Serializer, ROOT > {};
   template< typename Serializer, bool ROOT > struct is_well_ordered< boost::tuples::null_type, Serializer, ROOT > : boost::true_type {};

   template< typename HT, typename TT, typename Serializer, bool ROOT >
   struct is_well_ordered< boost::tuples::cons< HT, TT >, Serializer, ROOT >
   {
      static const bool value = is_well_ordered< HT, Serializer, ROOT >::value() && is_well_ordered< TT, Serializer, false >::value;
   };

   template< typename... Args, typename Serializer, bool ROOT >
   struct is_well_ordered< composite_key_compare< Args... >, Serializer, ROOT >
   {
      static const bool value = is_well_ordered< typename composite_key_compare< Args... >::key_comp_tuple, Serializer, ROOT >::value;
   };

   template< typename Key, typename CompareType, typename Serializer, bool ROOT >
   struct is_well_ordered< slice_comparator< Key, CompareType, Serializer >, Serializer, ROOT >
   {
      static const bool value = is_well_ordered< CompareType, Serializer, ROOT >::value;
   };

   MIRA_WELL_ORDERED_TYPE( char )
   MIRA_WELL_ORDERED_TYPE( uint16_t )
   MIRA_WELL_ORDERED_TYPE( uint32_t )
   MIRA_WELL_ORDERED_TYPE( uint64_t )

   MIRA_WELL_ORDERED_TYPE( fc::time_point_sec )
   MIRA_WELL_ORDERED_TYPE( fc::uint128_t )
   MIRA_WELL_ORDERED_TYPE( fc::sha256 )

   template< typename T, typename Serializer, bool ROOT > struct is_well_ordered< fc::safe< T >, Serializer, ROOT > : public is_well_ordered< T, Serializer, ROOT > {};
}
