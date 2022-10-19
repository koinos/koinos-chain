#include <koinos/exception.hpp>
#include <koinos/state_db/backends/iterator.hpp>

namespace koinos::state_db::backends {

iterator::iterator( std::unique_ptr< abstract_iterator > itr ) : _itr( std::move( itr ) ) {}

iterator::iterator( const iterator& other ) : _itr( other._itr->copy() ) {}

iterator::iterator( iterator&& other ) : _itr( std::move( other._itr ) ) {}

const iterator::value_type& iterator::operator*() const
{
   return **_itr;
}

const iterator::key_type& iterator::key() const
{
   return _itr->key();
}

iterator& iterator::operator++()
{
   ++(*_itr);
   return *this;
}

iterator& iterator::operator--()
{
   --(*_itr);
   return *this;
}

iterator& iterator::operator=( iterator&& other )
{
   _itr = std::move( other._itr );
   return *this;
}

bool iterator::valid() const
{
   return _itr && _itr->valid();
}

bool operator==( const iterator& x, const iterator& y )
{
   if ( x.valid() && y.valid() )
   {
      return *x == *y;
   }

   return x.valid() == y.valid();
}

bool operator!=( const iterator& x, const iterator& y )
{
   return !( x == y );
}

} // koinos::state_db::backends
