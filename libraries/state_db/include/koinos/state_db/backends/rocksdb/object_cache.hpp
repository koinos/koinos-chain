#pragma once

#include <koinos/state_db/backends/types.hpp>

#include <rocksdb/slice.h>

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace koinos::state_db::backends::rocksdb {

class object_cache
{
   public:
      using key_type       = detail::key_type;
      using value_type     = detail::value_type;

   private:
      using lru_list_type  = std::list< key_type >;
      using value_map_type =
         std::map<
            key_type,
            std::pair<
               std::shared_ptr< const value_type >,
               typename lru_list_type::iterator
            >
         >;

      lru_list_type     _lru_list;
      value_map_type    _object_map;
      std::size_t       _cache_size = 0;
      const std::size_t _cache_max_size;
      std::mutex        _mutex;

   public:
      object_cache( std::size_t size );
      ~object_cache();

      std::shared_ptr< const value_type > get( const key_type& k );
      std::shared_ptr< const value_type > get( const ::rocksdb::Slice& k );

      std::shared_ptr< const value_type > put( const key_type& k, const value_type& v );
      std::shared_ptr< const value_type > put( const ::rocksdb::Slice& k, const value_type& v );

      void remove( const key_type& k );
      void remove( const ::rocksdb::Slice& k );

      void clear();

      std::mutex& get_mutex();
};

} // koinos::state_db::backends::rocksdb
