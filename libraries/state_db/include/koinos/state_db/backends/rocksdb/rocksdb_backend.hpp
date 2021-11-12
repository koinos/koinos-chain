#pragma once

#include <koinos/state_db/backends/backend.hpp>
#include <koinos/state_db/backends/rocksdb/object_cache.hpp>
#include <koinos/state_db/backends/rocksdb/rocksdb_iterator.hpp>

#include <rocksdb/db.h>

#include <string>
#include <utility>

namespace koinos::state_db::backends::rocksdb {

// rocksmap is a RocksDB based map implementation for Koinos.
class rocksdb_backend final : public abstract_backend {
   public:
      using key_type   = abstract_backend::key_type;
      using value_type = abstract_backend::value_type;
      using size_type  = abstract_backend::size_type;

      rocksdb_backend();
      virtual ~rocksdb_backend();

      // Iterators
      virtual iterator begin();
      virtual iterator end();

      // Modifiers
      virtual bool put( const key_type& k, const value_type& v );
      virtual bool erase( const key_type& k );

      // Lookup
      virtual iterator find( const key_type& k );
      virtual iterator lower_bound( const key_type& k );
      virtual iterator upper_bound( const key_type& k );

   private:
      std::shared_ptr< ::rocksdb::DB >          _db;
      ::rocksdb::WriteOptions                   _wopts;
      std::shared_ptr< ::rocksdb::ReadOptions > _ropts;
      std::shared_ptr< object_cache >           _cache;
};

} // koinos::state_db::backends::rocksdb
