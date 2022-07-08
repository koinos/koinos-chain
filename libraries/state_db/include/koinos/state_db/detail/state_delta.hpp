#pragma once
#include <koinos/state_db/backends/backend.hpp>
#include <koinos/state_db/backends/map/map_backend.hpp>
#include <koinos/state_db/backends/rocksdb/rocksdb_backend.hpp>
#include <koinos/state_db/state_db_types.hpp>

#include <koinos/crypto/multihash.hpp>
#include <koinos/protocol/protocol.pb.h>

#include <any>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_set>

namespace koinos::state_db::detail {

   class state_delta : public std::enable_shared_from_this< state_delta >
   {
      public:
         using backend_type  = backends::abstract_backend;
         using key_type      = backend_type::key_type;
         using value_type    = backend_type::value_type;

      private:
         std::shared_ptr< state_delta >             _parent;

         std::shared_ptr< backend_type >            _backend;
         std::unordered_set< key_type >             _removed_objects;

         state_node_id                              _id;
         uint64_t                                   _revision = 0;
         mutable std::optional< crypto::multihash > _merkle_root;

         bool                                       _finalized = false;

         std::timed_mutex                           _cv_mutex;
         std::condition_variable_any                _cv;

      public:
         state_delta() = default;
         state_delta( const std::filesystem::path& p );
         ~state_delta() = default;

         void put( const key_type& k, const value_type& v );
         void erase( const key_type& k );
         const value_type* find( const key_type& key ) const;

         void squash();
         void commit();

         void clear();

         bool is_modified( const key_type& k ) const;
         bool is_removed( const key_type& k ) const;
         bool is_root() const;
         bool is_empty() const;

         uint64_t revision() const;
         void set_revision( uint64_t revision );

         bool is_finalized() const;
         void finalize();

         std::condition_variable_any& cv();
         std::timed_mutex& cv_mutex();

         crypto::multihash merkle_root() const;

         const state_node_id& id() const;
         const state_node_id& parent_id() const;
         std::shared_ptr< state_delta > parent() const;
         const protocol::block_header& block_header() const;

         std::shared_ptr< state_delta > make_child( const state_node_id& id = state_node_id(), const protocol::block_header& header = protocol::block_header() );

         const std::shared_ptr< backend_type > backend() const;

      private:
         void commit_helper();

         std::shared_ptr< state_delta > get_root();
   };

} // koinos::state_db::detail
