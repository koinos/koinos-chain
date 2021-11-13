#include <koinos/chain/event_recorder.hpp>

namespace koinos::chain {

void event_recorder::set_session( std::shared_ptr< abstract_event_session > s )
{
   _session = s;
}

void event_recorder::push_event( protocol::event_data&& ev )
{
   if ( auto session = _session.lock() )
   {
      session->push_event( std::move( ev ) );
   }
   else
   {
      _events.emplace_back( std::move( ev ) );
   }
}

const std::vector< protocol::event_data >& event_recorder::events()
{
   return _events;
}

} // koinos::chain
