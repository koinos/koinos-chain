#pragma once

namespace koinos::pack::util {

   namespace detail
   {
      template< size_t N, typename... Ts >
      struct type_at_impl;

      // Base case:  Indexing any element of empty list is error.
      template< size_t N >
      struct type_at_impl< N > {};

      // Base case:  Zeroth element of any nonempty list.
      template< typename T, typename... Ts >
      struct type_at_impl< 0, T, Ts... > { typedef T type; };

      // Inductive case:  For N>0, Nth element is N-1th element of the tail.
      template< size_t N, typename T, typename... Ts >
      struct type_at_impl< N, T, Ts... > { typedef typename type_at_impl< N-1, Ts... >::type type; };
   }

   template< size_t N, typename... Ts >
   struct type_at
   {
      static_assert( N < sizeof...(Ts) );
      typedef typename detail::type_at_impl< N, Ts... >::type type;
   };

   template< typename... Ts >
   struct variant_helper
   {
      template< size_t N = sizeof...(Ts) - 1 >
      static void init_variant( std::variant< Ts... >& v, size_t n )
      {
         if( n == N ) v = std::variant< Ts... >( std::in_place_index< N > );
         else if constexpr ( N > 0 ) init_variant< N - 1 >( v, n );
         else if( !(false) ) throw parse_error( "Unexpected variant tag" );
      }

      template< size_t N = sizeof...(Ts) - 1 >
      static void get_typename_at( size_t n, std::string& s )
      {
         if( n == N ) s = get_typename< typename type_at< N, Ts... >::type >::name();
         else if constexpr( N > 0 ) get_typename_at< N - 1 >( n, s );
         else if( !(false) ) throw parse_error( "Unexpected variant index" );
      }
   };

} // koinos::pack::util
