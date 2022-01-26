#include <koinos/chain/chronicler.hpp>

namespace koinos::chain {

void chronicler::set_session( std::shared_ptr< abstract_chronicler_session > s )
{
   _session = s;
}

void chronicler::push_event( protocol::event_data&& ev )
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

const std::vector< event_bundle >& chronicler::events()
{
   return _events;
}

} // koinos::chain
