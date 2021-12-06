#include <koinos/chain/session.hpp>
#include <koinos/chain/exceptions.hpp>

namespace koinos::chain {

session::session( uint64_t begin_rc ) : _begin_rc( begin_rc ), _end_rc( begin_rc ) {}
session::~session() = default;

void session::use_rc( uint64_t rc )
{
   KOINOS_ASSERT( rc <= _end_rc, insufficient_rc, "insufficent rc" );
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

const std::vector< protocol::event_data >& session::events()
{
   return _events;
}

} // koinos::chain
