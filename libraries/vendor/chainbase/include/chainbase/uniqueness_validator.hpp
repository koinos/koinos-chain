#pragma once
#include <boost/container/flat_set.hpp>
#include <boost/mpl/size.hpp>

namespace chainbase { namespace detail {

// This could probably be condensed in to a single function using boost::mpl

template< typename MultiIndexType, int N >
void check_uniqueness( MultiIndexType& i, const typename MultiIndexType::value_type& v, boost::container::flat_set< typename MultiIndexType::value_type::id_type >& ids )
{
   const auto& idx = i.template get< N >();

   auto itr = idx.find( v );
   if( itr != idx.end() )
      ids.insert( itr->id._id );
}

template< int N >
struct find_uniqueness_conflicts_impl
{
   template< typename MultiIndexType >
   static void find( MultiIndexType& i, const typename MultiIndexType::value_type& v, boost::container::flat_set< typename MultiIndexType::value_type::id_type >& ids )
   {
      check_uniqueness< MultiIndexType, N >( i, v, ids );
      find_uniqueness_conflicts_impl< N - 1 >::find( i, v, ids );
   }
};

template<>
struct find_uniqueness_conflicts_impl< 0 >
{
   template< typename MultiIndexType >
   static void find( MultiIndexType& i, const typename MultiIndexType::value_type& v, boost::container::flat_set< typename MultiIndexType::value_type::id_type >& ids )
   {
      check_uniqueness< MultiIndexType, 0 >( i, v, ids );
   }
};

} // detail

template< typename MultiIndexType >
void find_uniqueness_conflicts( MultiIndexType& i, const typename MultiIndexType::value_type& v, boost::container::flat_set< typename MultiIndexType::value_type::id_type >& ids )
{
   return detail::find_uniqueness_conflicts_impl< boost::mpl::size< typename MultiIndexType::mira_type::index_type_list >::type::value - 1 >::find( i, v, ids );
}

} // chainbase
