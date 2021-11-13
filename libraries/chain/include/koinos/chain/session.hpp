#pragma once

#include <koinos/chain/resource_meter.hpp>
#include <koinos/chain/event_recorder.hpp>

#include <cstdint>

namespace koinos::chain {

class session : public abstract_rc_session, public abstract_event_session
{
public:
   session( uint64_t begin_rc );
   ~session();

   virtual void use_rc( uint64_t rc ) override;
   virtual uint64_t remaining_rc() override;
   virtual uint64_t used_rc() override;
   virtual void push_event( protocol::event_data&& ev ) override;
   const std::vector< protocol::event_data >& events() override;

private:
   uint64_t                            _begin_rc;
   uint64_t                            _end_rc;
   std::vector< protocol::event_data > _events;
};

} // koinos::chain
