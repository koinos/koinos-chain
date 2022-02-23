#pragma once

#include <koinos/state_db/state_db.hpp>
#include <koinos/mq/client.hpp>

#include <koinos/vm_manager/vm_backend.hpp>

#include <memory>

namespace koinos::chain {

class controller;

class pending_state final
{
public:
   void set_client( std::shared_ptr< mq::client > client );
   void set_backend( std::shared_ptr< vm_manager::vm_backend > backend );
   state_db::anonymous_state_node_ptr get_state_node();
   void rebuild( state_db::state_node_ptr head, const protocol::block& cached_head_block );
   void close();

private:
   std::shared_ptr< vm_manager::vm_backend > _backend;
   std::shared_ptr< mq::client >             _client;
   state_db::anonymous_state_node_ptr        _pending_state;
};

} // koinos::chain
