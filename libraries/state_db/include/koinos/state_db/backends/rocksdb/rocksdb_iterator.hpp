#pragma once

#include <koinos/state_db/backends/iterator.hpp>
#include <koinos/state_db/backends/rocksdb/object_cache.hpp>

#include <rocksdb/db.h>

#include <string>

namespace koinos::state_db::backends::rocksdb {

class rocksdb_backend;

class rocksdb_iterator final : public abstract_iterator
{
   public:
      using value_type = abstract_iterator::value_type;

      rocksdb_iterator( std::shared_ptr< ::rocksdb::DB > db, std::shared_ptr< const ::rocksdb::ReadOptions > opts, std::shared_ptr< object_cache > cache );
      rocksdb_iterator( const rocksdb_iterator& other );
      virtual ~rocksdb_iterator();

      virtual const value_type& operator*()const;

      virtual const key_type& key()const;

      virtual abstract_iterator& operator++();
      virtual abstract_iterator& operator--();

   private:
      friend class rocksdb_backend;

      virtual bool valid()const;
      virtual std::unique_ptr< abstract_iterator > copy()const;

      void update_cache_value();

      std::shared_ptr< ::rocksdb::DB >                _db;
      std::unique_ptr< ::rocksdb::Iterator >          _iter;
      std::shared_ptr< const ::rocksdb::ReadOptions > _opts;
      std::shared_ptr< object_cache >                 _cache;
      std::shared_ptr< const value_type >             _cache_value;
      std::shared_ptr< const key_type >               _key;
};

} // koinos::state_db::backends::rocksdb
