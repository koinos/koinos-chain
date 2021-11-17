#pragma once

#include <koinos/state_db/backends/backend.hpp>
#include <koinos/state_db/backends/rocksdb/object_cache.hpp>
#include <koinos/state_db/backends/rocksdb/rocksdb_iterator.hpp>

#include <rocksdb/db.h>

#include <filesystem>
#include <string>
#include <utility>

namespace koinos::state_db::backends::rocksdb {

class rocksdb_backend final : public abstract_backend {
   public:
      using key_type   = abstract_backend::key_type;
      using value_type = abstract_backend::value_type;
      using size_type  = abstract_backend::size_type;

      rocksdb_backend();
      virtual ~rocksdb_backend();

      void open( const std::filesystem::path& p );
      void close();

      size_type revision()const;
      void set_revision( size_type rev );

      // Iterators
      virtual iterator begin();
      virtual iterator end();

      // Modifiers
      virtual void put( const key_type& k, const value_type& v );
      virtual void erase( const key_type& k );
      virtual void clear();

      virtual size_type size()const;

      // Lookup
      virtual iterator find( const key_type& k );
      virtual iterator lower_bound( const key_type& k );

   private:
      bool maybe_create_columns( const std::filesystem::path& p );

      std::shared_ptr< ::rocksdb::DB >              _db;
      std::vector< ::rocksdb::ColumnFamilyHandle* > _handles;
      ::rocksdb::WriteOptions                       _wopts;
      std::shared_ptr< ::rocksdb::ReadOptions >     _ropts;
      std::shared_ptr< object_cache >               _cache;
      size_type                                     _size = 0;
      size_type                                     _revision = 0;
};

} // koinos::state_db::backends::rocksdb