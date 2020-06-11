
#include <koinos/chain/xcalls.hpp>

#include <koinos/pack/classes.hpp>

#include <koinos/pack/rt/binary.hpp>

#include <koinos/statedb/statedb_types.hpp>

namespace koinos { namespace chain {

using koinos::protocol::thunk_id_type;
using koinos::protocol::xcall_target;

vl_blob get_default_xcall_entry( uint32_t xid )
{
   vl_blob ret;
   switch( xid )
   {
      case 2345:
      {
         xcall_target target = thunk_id_type( 1234 );
         koinos::pack::to_vl_blob( ret, target );
         break;
      }
      default:
      {
         // do nothing.
      }
   }

   return ret;
}

} }
