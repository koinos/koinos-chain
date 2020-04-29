#pragma once
#include "fixture_fwd.hpp"

#include <chainbase/chainbase.hpp>

#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>

namespace fc
{

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

namespace raw
{

template<typename Stream, typename T>
void pack( Stream& s, const chainbase::oid<T>& id )
{
   s.write( (const char*)&id._id, sizeof(id._id) );
}

template<typename Stream, typename T>
void unpack( Stream& s, chainbase::oid<T>& id )
{
   s.read( (char*)&id._id, sizeof(id._id));
}

} } // fc::raw

#include <mira/index_adapter.hpp>
#include <mira/multi_index_container.hpp>
#include <mira/ordered_index.hpp>
#include <mira/tag.hpp>
#include <mira/member.hpp>
#include <mira/indexed_by.hpp>
#include <mira/composite_key.hpp>
#include <mira/mem_fun.hpp>

#include "example_fixture.hpp"

using namespace chainbase;

template< typename... Args >
using multi_index_container = mira::multi_index_adapter< Args... >;
using mira::multi_index::indexed_by;
using mira::multi_index::ordered_unique;
using mira::multi_index::tag;
using mira::multi_index::member;
using mira::multi_index::composite_key;
using mira::multi_index::composite_key_compare;

typedef example_fixture chainbase_fixture;

enum test_object_type
{
   book_object_type
};

struct book : public chainbase::object< book_object_type, book > {

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
   mira::multi_index::indexed_by<
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_id >, mira::multi_index::member< book, book::id_type, &book::id > >,
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_a >,  mira::multi_index::member< book, int,           &book::a  > >,
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_b >,
         mira::multi_index::composite_key< book,
            mira::multi_index::member< book, int, &book::b >,
            mira::multi_index::member< book, int, &book::a >
         >,
         mira::multi_index::composite_key_compare< std::less< int >, std::less< int > >
      >,
      mira::multi_index::ordered_unique< mira::multi_index::tag< by_sum >, mira::multi_index::const_mem_fun< book, int, &book::sum > >
  >
> book_index;

FC_REFLECT( book::id_type, (_id) )
FC_REFLECT( book, (id)(a)(b) )
CHAINBASE_SET_INDEX_TYPE( book, book_index )
