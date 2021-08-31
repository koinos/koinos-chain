#pragma once

#include <algorithm>
#include <array>

#include <mira/index_adapter.hpp>
#include <mira/ordered_index.hpp>
#include <mira/tag.hpp>
#include <mira/member.hpp>
#include <mira/indexed_by.hpp>
#include <mira/composite_key.hpp>
#include <mira/mem_fun.hpp>

namespace detail {

template< typename H, typename T >
std::vector< char > to_binary_vector( const boost::tuples::cons< H, T >& var );

template< typename H, typename T >
std::size_t from_binary_array( const char* data, std::size_t size, boost::tuples::cons< H, T >& var );

template< typename T >
std::vector< char > to_binary_vector( const mira::multi_index::composite_key_result< T >& var );

template< typename T >
std::size_t from_binary_array( const char* data, std::size_t size, mira::multi_index::composite_key_result< T >& var );

std::vector< char > to_binary_vector( const boost::tuples::null_type& var );

std::size_t from_binary_array( const char* data, std::size_t size, boost::tuples::null_type& var );

} // detail

struct test_serializer {
   static inline std::vector< char > to_binary_vector( const boost::tuples::null_type )
   {
      return std::vector< char >();
   }

   static inline std::size_t from_binary_array( const char*, std::size_t, boost::tuples::null_type )
   {
      return 0;
   }

   template< typename T >
   static inline std::enable_if_t< std::is_trivially_copyable_v< T >, std::vector< char > >
   to_binary_vector( const T& v )
   {
      std::vector< char > bytes;
      bytes.resize( sizeof( T ) );
      std::copy( reinterpret_cast< const char* >( &v ),  reinterpret_cast< const char* >( &v ) + sizeof( T ), bytes.begin() );
      return bytes;
   }

   template< typename T >
   static inline std::enable_if_t< std::is_trivially_copyable_v< T >, std::size_t >
   from_binary_array( const char* data, std::size_t size, T& t )
   {
      assert( sizeof( T ) <= size );
      char* obj = reinterpret_cast< char* >( std::addressof( t ) );
      std::copy( data, data + sizeof( T ), obj );
      return sizeof( T );
   }

   template< typename T >
   static inline std::enable_if_t< !std::is_trivially_copyable_v< T >, std::vector< char > >
   to_binary_vector( const T& v )
   {
      return detail::to_binary_vector( v );
   }

   template< typename T >
   static inline std::enable_if_t< !std::is_trivially_copyable_v< T >, std::size_t >
   from_binary_array( const char* data, std::size_t size, T& t )
   {
      return detail::from_binary_array( data, size, t );
   }

   template< typename T >
   static inline T from_binary_array( const char* data, std::size_t size )
   {
      T t;
      from_binary_array( data, size, t );
      return t;
   }

   template< typename T >
   static inline size_t binary_size( const T& v )
   {
      return sizeof( T );
   }
};

namespace detail {

template< typename H, typename T >
std::vector< char > to_binary_vector( const boost::tuples::cons< H, T >& var )
{
   auto a = test_serializer::to_binary_vector( var.get_head() );
   auto b = test_serializer::to_binary_vector( var.get_tail() );

   std::vector< char > ab;
   ab.reserve( a.size() + b.size() );
   ab.insert( ab.end(), a.begin(), a.end() );
   ab.insert( ab.end(), b.begin(), b.end() );

   return ab;
}

template< typename H, typename T >
std::size_t from_binary_array( const char* data, std::size_t size, boost::tuples::cons< H, T >& var )
{
   auto a = test_serializer::from_binary_array( data, size, var.get_head() );
   auto b = test_serializer::from_binary_array( data + a, size - a, var.get_tail() );

   return a + b;
}

template< typename T >
std::vector< char > to_binary_vector( const mira::multi_index::composite_key_result< T >& var )
{
   return test_serializer::to_binary_vector( var.key );
}

template< typename T >
std::size_t from_binary_array( const char* data, std::size_t size, mira::multi_index::composite_key_result< T >& var )
{
   return test_serializer::from_binary_array( data, size, var.key );
}

std::vector< char > to_binary_vector( const boost::tuples::null_type& var )
{
   return std::vector< char >();
}

std::size_t from_binary_array( const char* data, std::size_t size, boost::tuples::null_type& var )
{
   return 0;
}

} // detail

enum test_object_type
{
   book_object_type,
   single_index_object_type,
   test_object_type,
   test_object2_type,
   test_object3_type,
   account_object_type
};

struct book
{
   typedef uint64_t id_type;

   id_type id = 0;
   int     a  = 0;
   int     b  = 1;

   int sum()const { return a + b; }
};

struct by_id;
struct by_a;
struct by_b;
struct by_sum;

typedef mira::multi_index_adapter<
   book,
   test_serializer,
   mira::multi_index::indexed_by<
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_id >, mira::multi_index::member< book, book::id_type, &book::id > >,
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_a >,  mira::multi_index::member< book, int,           &book::a  > >,
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_b >,
         mira::multi_index::composite_key< book,
            mira::multi_index::member< book, int, &book::b >,
            mira::multi_index::member< book, int, &book::a >
         >,
         mira::multi_index::composite_key_compare< std::greater< int >, std::less< int > >
      >,
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_sum >, mira::multi_index::const_mem_fun< book, int, &book::sum > >
  >
> book_index;

struct single_index_object
{
   typedef uint64_t id_type;

   id_type id = 0;
};

typedef mira::multi_index_adapter<
   single_index_object,
   test_serializer,
   mira::multi_index::indexed_by<
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_id >, mira::multi_index::member< single_index_object, single_index_object::id_type, &single_index_object::id > >
   >
> single_index_index;

struct test_object
{
   typedef uint64_t id_type;

   id_type                 id   = 0;
   uint32_t                val  = 0;
   uint32_t                val2 = 0;
};

struct ordered_idx;
struct composited_ordered_idx;


typedef mira::multi_index_adapter<
   test_object,
   test_serializer,
   mira::multi_index::indexed_by<
      mira::multi_index::ordered_unique< mira::multi_index::tag< ordered_idx >, mira::multi_index::member< test_object, test_object::id_type, &test_object::id > >,
      mira::multi_index::ordered_unique< mira::multi_index::tag< composited_ordered_idx >,
         mira::multi_index::composite_key< test_object,
            mira::multi_index::member< test_object, uint32_t, &test_object::val2 >,
            mira::multi_index::member< test_object, uint32_t, &test_object::val >
         >
      >
   >
> test_object_index;

struct test_object2
{
   typedef uint64_t id_type;

   id_type  id  = 0;
   uint32_t val = 0;
};

struct ordered_idx2;
struct composite_ordered_idx2;

typedef mira::multi_index_adapter<
   test_object2,
   test_serializer,
   mira::multi_index::indexed_by<
      mira::multi_index::ordered_unique< mira::multi_index::tag< ordered_idx2 >, mira::multi_index::member< test_object2, test_object2::id_type, &test_object2::id > >,
      mira::multi_index::ordered_unique< mira::multi_index::tag< composite_ordered_idx2 >,
         mira::multi_index::composite_key< test_object2,
            mira::multi_index::member< test_object2, uint32_t, &test_object2::val >,
            mira::multi_index::member< test_object2, test_object2::id_type, &test_object2::id >
         >
      >
   >
> test_object2_index;

struct test_object3
{
   typedef uint64_t id_type;

   id_type  id   = 0;
   uint32_t val  = 0;
   uint32_t val2 = 0;
   uint32_t val3 = 0;
};

struct ordered_idx3;
struct composite_ordered_idx3a;
struct composite_ordered_idx3b;

typedef mira::multi_index_adapter<
   test_object3,
   test_serializer,
   mira::multi_index::indexed_by<
      mira::multi_index::ordered_unique< mira::multi_index::tag< ordered_idx3 >, mira::multi_index::member< test_object3, test_object3::id_type, &test_object3::id > >,
      mira::multi_index::ordered_unique< mira::multi_index::tag< composite_ordered_idx3a >,
         mira::multi_index::composite_key< test_object3,
            mira::multi_index::member< test_object3, uint32_t, &test_object3::val >,
            mira::multi_index::member< test_object3, uint32_t, &test_object3::val2 >
         >
      >,
      mira::multi_index::ordered_unique< mira::multi_index::tag< composite_ordered_idx3b >,
         mira::multi_index::composite_key< test_object3,
            mira::multi_index::member< test_object3, uint32_t, &test_object3::val >,
            mira::multi_index::member< test_object3, uint32_t, &test_object3::val3 >
         >
      >
   >
> test_object3_index;


struct account_object
{
   typedef uint64_t id_type;

   id_type                 id = 0;
   std::array< char, 256 > name;
};

struct by_name;

struct name_comparator
{
   bool operator()( const std::array< char, 256 >& a, const std::array< char, 256 >& b )const
   {
      return std::string( a.data() ) < std::string( b.data() );
   }
};

typedef mira::multi_index_adapter<
   account_object,
   test_serializer,
   mira::multi_index::indexed_by<
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_id >, mira::multi_index::member< account_object, account_object::id_type, &account_object::id > >,
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_name >, mira::multi_index::member< account_object, std::array< char, 256 >, &account_object::name >, name_comparator >
   >
> account_index;
