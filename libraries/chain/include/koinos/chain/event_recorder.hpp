#pragma once
#include <koinos/protocol/protocol.pb.h>
#include <memory>
#include <utility>

namespace koinos::chain {

using event_bundle = std::pair< bool, protocol::event_data >;

struct abstract_event_session
{
   virtual void push_event( const protocol::event_data& ev )   = 0;
   virtual const std::vector< protocol::event_data >& events() = 0;
};

class event_recorder final {
public:
   void set_session( std::shared_ptr< abstract_event_session > s );
   void push_event( protocol::event_data&& ev );
   const std::vector< event_bundle >& events();
private:
   std::weak_ptr< abstract_event_session > _session;
   std::vector< event_bundle >             _events;
   uint32_t                                _seq_no = 0;
};

} // koinos::chain
