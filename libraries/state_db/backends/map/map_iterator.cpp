#include <koinos/state_db/backends/map/map_iterator.hpp>

#include <koinos/exception.hpp>

namespace koinos::state_db::backends::map {

map_iterator::map_iterator( std::unique_ptr< std::map< detail::key_type, detail::value_type >::iterator > itr,
                            const std::map< detail::key_type, detail::value_type >& map ) :
   _itr( std::move( itr ) ),
   _map( map )
   {}

map_iterator::~map_iterator() {}

const map_iterator::value_type& map_iterator::operator*()const
{
   KOINOS_ASSERT( valid(), koinos::exception, "" );
   return (*_itr)->second;
}

const map_iterator::key_type& map_iterator::key()const
{
   KOINOS_ASSERT( valid(), koinos::exception, "" );
   return (*_itr)->first;
}

abstract_iterator& map_iterator::operator++()
{
   KOINOS_ASSERT( valid(), koinos::exception, "" );
   ++(*_itr);
   return *this;
}

abstract_iterator& map_iterator::operator--()
{
   --(*_itr);
   return *this;
}

bool map_iterator::valid()const
{
   return _itr && *_itr != _map.end();
}

std::unique_ptr< abstract_iterator > map_iterator::copy()const
{
   return std::make_unique< map_iterator >( std::make_unique< std::map< detail::key_type, detail::value_type >::iterator >( *_itr ), _map );
}

} // koinos::state_db::backends::map
