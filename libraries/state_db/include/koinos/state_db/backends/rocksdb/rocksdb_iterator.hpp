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

      rocksdb_iterator(
         std::shared_ptr< ::rocksdb::DB > db,
         std::shared_ptr< ::rocksdb::ColumnFamilyHandle > handle,
         std::shared_ptr< const ::rocksdb::ReadOptions > opts,
         std::shared_ptr< object_cache > cache );
      rocksdb_iterator( const rocksdb_iterator& other );
      virtual ~rocksdb_iterator() override;

      virtual const value_type& operator*() const override;

      virtual const key_type& key() const override;

      virtual abstract_iterator& operator++() override;
      virtual abstract_iterator& operator--() override;

   private:
      friend class rocksdb_backend;

      virtual bool valid() const override;
      virtual std::unique_ptr< abstract_iterator > copy() const override;

      void update_cache_value() const;

      std::shared_ptr< ::rocksdb::DB >                 _db;
      std::shared_ptr< ::rocksdb::ColumnFamilyHandle > _handle;
      std::unique_ptr< ::rocksdb::Iterator >           _iter;
      std::shared_ptr< const ::rocksdb::ReadOptions >  _opts;
      mutable std::shared_ptr< object_cache >          _cache;
      mutable std::shared_ptr< const value_type >      _cache_value;
      mutable std::shared_ptr< const key_type >        _key;
};

} // koinos::state_db::backends::rocksdb
