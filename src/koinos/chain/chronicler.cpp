#include <koinos/chain/chronicler.hpp>

namespace koinos::chain {

void chronicler::set_session( std::shared_ptr< abstract_chronicler_session > s )
{
  _session = s;
}

void chronicler::push_event( std::optional< std::string > transaction_id, protocol::event_data&& ev )
{
  ev.set_sequence( _seq_no );

  if( auto session = _session.lock() )
    session->push_event( ev );

  _events.emplace_back( std::make_pair( transaction_id, std::move( ev ) ) );
  _seq_no++;
}

void chronicler::push_log( const std::string& message )
{
  if( auto session = _session.lock() )
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

} // namespace koinos::chain
