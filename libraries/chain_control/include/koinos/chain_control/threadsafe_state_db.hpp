
#pragma once

#include <koinos/statedb/statedb.hpp>

#include <mutex>

namespace koinos { namespace chain_control {

/**
 * Simple thread-safe wrapper for statedb.
 * Does not attempt to allow actual concurrent access, all methods are locked with a mutex.
 * This header will go away when the DB actually supports concurrent access.
 */
class threadsafe_state_db final
{
   public:
      threadsafe_state_db() {}
      ~threadsafe_state_db() {}

      void open( const boost::filesystem::path& p, const boost::any& o )
      {
         std::lock_guard< std::mutex > lock( _state_db_mutex );
         _state_db.open( p, o );
      }

      void close()
      {
         std::lock_guard< std::mutex > lock( _state_db_mutex );
         _state_db.close();
      }

      std::shared_ptr< StateNode > get_empty_node()
      {
         std::lock_guard< std::mutex > lock( _state_db_mutex );
         return _state_db.get_empty_node();
      }

      void get_recent_states(std::vector<StateNodeId>& node_id_list, int limit)
      {
         std::lock_guard< std::mutex > lock( _state_db_mutex );
         _state_db.get_recent_states( node_id_list, limit );
      }

      std::shared_ptr< StateNode > get_node( StateNodeId node_id )
      {
         std::lock_guard< std::mutex > lock( _state_db_mutex );
         return _state_db.get_node( node_id );
      }

      std::shared_ptr< StateNode > create_writable_node( StateNodeId parent_id )
      {
         std::lock_guard< std::mutex > lock( _state_db_mutex );
         return _state_db.create_writable_node( parent_id );
      }

      void finalize_node( StateNodeId node_id )
      {
         std::lock_guard< std::mutex > lock( _state_db_mutex );
         _state_db.finalize_node( node_id );
      }

      void discard_node( StateNodeId node_id )
      {
         std::lock_guard< std::mutex > lock( _state_db_mutex );
         _state_db.discard_node( node_id );
      }

   private:
      StateDB           _state_db;
      std::mutex        _state_db_mutex;
};

} }
