#pragma once

#include <koinos/state_db/backends/iterator.hpp>

namespace koinos::state_db::backends {

class abstract_backend
{
   public:
      using key_type   = detail::key_type;
      using value_type = detail::value_type;
      using size_type  = detail::size_type;

      virtual ~abstract_backend() {};

      virtual iterator begin() = 0;
      virtual iterator end() = 0;

      virtual void put( const key_type& k, const value_type& v ) = 0;
      virtual void erase( const key_type& k ) = 0;

      virtual iterator find( const key_type& k ) = 0;
      virtual iterator lower_bound( const key_type& k ) = 0;
};

} // koinos::state_db::backends
