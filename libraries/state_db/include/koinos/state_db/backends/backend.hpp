#pragma once

#include <koinos/state_db/backends/iterator.hpp>

#include <string>

namespace koinos::state_db::backends {

class abstract_backend
{
   public:
      using key_type   = std::pair< std::string, std::string >;
      using value_type = std::string;
      using size_type  = std::size_t;

      virtual ~abstract_backend() {};

      virtual iterator begin() = 0;
      virtual iterator end() = 0;

      virtual bool put( const key_type& k, const value_type& v ) = 0;
      virtual bool erase( const key_type& k ) = 0;

      virtual iterator find( const key_type& k ) = 0;
      virtual iterator lower_bound( const key_type& k ) = 0;
      virtual iterator upper_bound( const key_type& k ) = 0;
};

} // koinos::state_db::backends
