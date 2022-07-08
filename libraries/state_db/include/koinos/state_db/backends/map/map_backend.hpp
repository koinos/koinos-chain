#pragma once

#include <koinos/state_db/backends/backend.hpp>
#include <koinos/state_db/backends/map/map_iterator.hpp>

namespace koinos::state_db::backends::map {

class map_backend final : public abstract_backend {
   public:
      using key_type   = abstract_backend::key_type;
      using value_type = abstract_backend::value_type;
      using size_type  = abstract_backend::size_type;

      map_backend();
      virtual ~map_backend() override;

      // Iterators
      virtual iterator begin() noexcept override;
      virtual iterator end() noexcept override;

      // Modifiers
      virtual void put( const key_type& k, const value_type& v ) override;
      virtual const value_type* get( const key_type& ) const override;
      virtual void erase( const key_type& k ) override;
      virtual void clear() noexcept override;

      virtual size_type size() const noexcept override;

      // Lookup
      virtual iterator find( const key_type& k ) override;
      virtual iterator lower_bound( const key_type& k ) override;

      virtual const protocol::block_header& block_header() const override;
      virtual void set_block_header( const protocol::block_header& ) override;

   private:
      std::map< key_type, value_type > _map;
      protocol::block_header           _header;
};

} // koinos::state_db::backends::map
