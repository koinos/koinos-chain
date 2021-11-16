#pragma once

#include <koinos/state_db/backends/types.hpp>

namespace koinos::state_db::backends {

class iterator;

class abstract_iterator
{
   public:
      using key_type   = detail::key_type;
      using value_type = detail::value_type;

      virtual ~abstract_iterator() {};

      virtual const value_type& operator*()const = 0;

      virtual const key_type& key()const = 0;

      virtual abstract_iterator& operator++() = 0;
      virtual abstract_iterator& operator--() = 0;

   private:
      friend class iterator;

      virtual bool valid()const = 0;
      virtual std::unique_ptr< abstract_iterator > copy()const = 0;
};

class iterator final
{
   public:
      using key_type   = detail::key_type;
      using value_type = detail::value_type;

      iterator( std::unique_ptr< abstract_iterator > );
      iterator( const iterator& other );
      iterator( iterator&& other );

      const value_type& operator*()const;
      const value_type* operator->()const;

      const key_type& key()const;

      iterator& operator++();
      iterator& operator--();

      iterator& operator=( const iterator& other );
      iterator& operator=( iterator&& other );

      friend bool operator==( const iterator& x, const iterator& y );
      friend bool operator!=( const iterator& x, const iterator& y );

   private:
      bool valid()const;

      std::unique_ptr< abstract_iterator > _itr;
};

} // koinos::state_db::backends
