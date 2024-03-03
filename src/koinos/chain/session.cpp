#include <koinos/chain/session.hpp>
#include <koinos/chain/exceptions.hpp>

namespace koinos::chain {

session::session( int64_t begin_rc ) : _begin_rc( begin_rc ), _end_rc( begin_rc ) {}
session::~session() = default;

void session::use_rc( int64_t rc )
{
   KOINOS_ASSERT( rc <= _end_rc, insufficient_rc_exception, "insufficient rc" );
   _end_rc -= rc;
}

uint64_t session::remaining_rc()
{
   return uint64_t( std::min( _begin_rc, _end_rc ) );
}

uint64_t session::used_rc()
{
   if ( _end_rc > _begin_rc )
      return 0;

   return uint64_t( _begin_rc - _end_rc );
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
