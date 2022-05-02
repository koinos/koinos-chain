#include <koinos/chain/session.hpp>
#include <koinos/chain/exceptions.hpp>

namespace koinos::chain {

session::session( uint64_t begin_rc ) : _begin_rc( begin_rc ), _end_rc( begin_rc ) {}
session::~session() = default;

void session::use_rc( uint64_t rc )
{
   KOINOS_ASSERT_REVERSION( rc <= _end_rc, "insufficent rc" );
   _end_rc -= rc;
}

uint64_t session::remaining_rc()
{
   return _end_rc;
}

uint64_t session::used_rc()
{
   return _begin_rc - _end_rc;
}

void session::push_event( const protocol::event_data& ev )
{
   _events.push_back( ev );
}

void session::push_log( const std::string& log )
{
   _logs.push_back( log );
}

const std::vector< protocol::event_data >& session::events()
{
   return _events;
}

const std::vector< std::string >& session::logs()
{
   return _logs;
}

} // koinos::chain
