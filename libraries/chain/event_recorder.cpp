#include <koinos/chain/event_recorder.hpp>

namespace koinos::chain {

void event_recorder::set_session( std::shared_ptr< abstract_event_session > s )
{
   _session = s;
}

void event_recorder::push_event( protocol::event_data&& ev )
{
   ev.set_sequence( _seq_no );
   bool within_session = false;

   if ( auto session = _session.lock() )
   {
      within_session = true;
      session->push_event( ev );
   }

   _events.emplace_back( std::make_pair( within_session, std::move( ev ) ) );
   _seq_no++;
}

const std::vector< event_bundle >& event_recorder::events()
{
   return _events;
}

} // koinos::chain
