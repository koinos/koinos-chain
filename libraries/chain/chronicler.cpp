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

void chronicler::push_log( const std::string& message )
{
   if ( auto session = _session.lock() )
      session->push_log( message );
   else
      _logs.push_back( message );
}

const std::vector< event_bundle >& chronicler::events()
{
   return _events;
}

const std::vector< std::string >& chronicler::logs()
{
   return _logs;
}

} // koinos::chain
