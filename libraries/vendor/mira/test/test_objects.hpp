#pragma once

#include <koinos/pack/rt/reflect.hpp>
#include <koinos/pack/rt/binary_serializer.hpp>

#include <mira/index_adapter.hpp>
#include <mira/ordered_index.hpp>
#include <mira/tag.hpp>
#include <mira/member.hpp>
#include <mira/indexed_by.hpp>
#include <mira/composite_key.hpp>
#include <mira/mem_fun.hpp>

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

   template<typename Constructor, typename Allocator>
   book( Constructor&& c, Allocator&& a )
   {
      c(*this);
   }

   book() = default;

   id_type id;
   int a = 0;
   int b = 1;

   int sum()const { return a + b; }
};

struct by_id;
struct by_a;
struct by_b;
struct by_sum;

typedef mira::multi_index_adapter<
   book,
   koinos::pack::binary_serializer,
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

   template< typename Constructor, typename Allocator >
   single_index_object( Constructor&& c, Allocator&& a )
   {
      c( *this );
   }

   single_index_object() = default;

   id_type id;
};

typedef mira::multi_index_adapter<
   single_index_object,
   koinos::pack::binary_serializer,
   mira::multi_index::indexed_by<
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_id >, mira::multi_index::member< single_index_object, single_index_object::id_type, &single_index_object::id > >
   >
> single_index_index;

struct test_object
{
   typedef uint64_t id_type;

   template <class Constructor, class Allocator>
   test_object(Constructor&& c, Allocator a ) : id( 0 ), val( 0 )
   {
      c(*this);
   }

   template <class Constructor, class Allocator>
   test_object(Constructor&& c, int64_t _id, Allocator a ) : id( _id ), val( 0 ), name( a )
   {
      c(*this);
   }

   test_object() = default;

   id_type id;
   uint32_t val;
   std::string name;
};

struct ordered_idx;
struct composited_ordered_idx;

typedef mira::multi_index_adapter<
   test_object,
   koinos::pack::binary_serializer,
   mira::multi_index::indexed_by<
      mira::multi_index::ordered_unique< mira::multi_index::tag< ordered_idx >, mira::multi_index::member< test_object, test_object::id_type, &test_object::id > >,
      mira::multi_index::ordered_unique< mira::multi_index::tag< composited_ordered_idx >,
         mira::multi_index::composite_key< test_object,
            mira::multi_index::member< test_object, std::string, &test_object::name >,
            mira::multi_index::member< test_object, uint32_t, &test_object::val >
         >
      >
   >
> test_object_index;

struct test_object2
{
   typedef uint64_t id_type;

   template <class Constructor, class Allocator>
   test_object2(Constructor&& c, Allocator a ) : id( 0 ), val( 0 )
   {
      c(*this);
   }

   test_object2() = default;

   id_type id;
   uint32_t val;
};

struct ordered_idx2;
struct composite_ordered_idx2;

typedef mira::multi_index_adapter<
   test_object2,
   koinos::pack::binary_serializer,
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

   template <class Constructor, class Allocator>
   test_object3(Constructor&& c, Allocator a ) : id( 0 ), val( 0 ), val2( 0 ), val3( 0 )
   {
      c(*this);
   }

   test_object3() = default;

   id_type id;
   uint32_t val;
   uint32_t val2;
   uint32_t val3;
};

struct ordered_idx3;
struct composite_ordered_idx3a;
struct composite_ordered_idx3b;

typedef mira::multi_index_adapter<
   test_object3,
   koinos::pack::binary_serializer,
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

   template< typename Constructor, typename Allocator >
   account_object( Constructor&& c, Allocator&& a )
   {
      c( *this );
   }

   account_object() = default;

   id_type id;
   std::string name;
};

struct by_name;

typedef mira::multi_index_adapter<
   account_object,
   koinos::pack::binary_serializer,
   mira::multi_index::indexed_by<
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_id >, mira::multi_index::member< account_object, account_object::id_type, &account_object::id > >,
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_name >, mira::multi_index::member< account_object, std::string, &account_object::name > >
   >
> account_index;

KOINOS_REFLECT( book, (id)(a)(b) )
KOINOS_REFLECT( single_index_object, (id) )
KOINOS_REFLECT( test_object, (id)(val)(name) )
KOINOS_REFLECT( test_object2, (id)(val) )
KOINOS_REFLECT( test_object3, (id)(val)(val2)(val3) )
KOINOS_REFLECT( account_object, (id)(name) )
