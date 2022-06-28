#include <koinos/chain/state.hpp>
#include <koinos/chain/object_spaces.pb.h>
#include <koinos/chain/system_calls.hpp>
#include <koinos/log.hpp>
#include <koinos/util/hex.hpp>

namespace koinos::chain {

bool operator<( const object_space& lhs, const object_space& rhs )
{
   if ( lhs.system() < rhs.system() )
   {
      return true;
   }
   else if ( lhs.system() > rhs.system() )
   {
      return false;
   }

   if ( lhs.zone() < rhs.zone() )
   {
      return true;
   }
   else if ( lhs.system() > rhs.system() )
   {
      return false;
   }

   return lhs.id() < rhs.id();
}

namespace state {

namespace space {

namespace detail {

const object_space make_contract_bytecode()
{
   object_space s;
   s.set_system( true );
   s.set_zone( zone::kernel );
   s.set_id( system_space_id::contract_bytecode );
   return s;
}

const object_space make_contract_metadata()
{
   object_space s;
   s.set_system( true );
   s.set_zone( zone::kernel );
   s.set_id( system_space_id::contract_metadata );
   return s;
}

const object_space make_system_call_dispatch()
{
   object_space s;
   s.set_system( true );
   s.set_zone( zone::kernel );
   s.set_id( system_space_id::system_call_dispatch );
   return s;
}

const object_space make_metadata()
{
   object_space s;
   s.set_system( true );
   s.set_zone( zone::kernel );
   s.set_id( system_space_id::metadata );
   return s;
}

const object_space make_transaction_nonce()
{
   object_space s;
   s.set_system( true );
   s.set_zone( zone::kernel );
   s.set_id( system_space_id::transaction_nonce );
   return s;
}

} // detail

const object_space contract_bytecode()
{
   static auto s = detail::make_contract_bytecode();
   return s;
}

const object_space contract_metadata()
{
   static auto s = detail::make_contract_metadata();
   return s;
}

const object_space system_call_dispatch()
{
   static auto s = detail::make_system_call_dispatch();
   return s;
}

const object_space metadata()
{
   static auto s = detail::make_metadata();
   return s;
}

const object_space transaction_nonce()
{
   static auto s = detail::make_transaction_nonce();
   return s;
}

} // space

void assert_permissions( execution_context& context, const object_space& space )
{
   if ( context.get_caller_privilege() == privilege::kernel_mode )
   {
      KOINOS_ASSERT( space.system(), reversion_exception, "privileged code can only access system space" );
   }
   else
   {
      KOINOS_ASSERT( !space.system(), insufficient_privileges_exception, "user code cannot access system space" );
      KOINOS_ASSERT( space.zone() == context.get_caller(), reversion_exception, "user code cannot access other contract space" );
   }
}

} // state

} // koinos::chain
