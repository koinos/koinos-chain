#include <koinos/chain/state.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/log.hpp>

namespace koinos::chain::state {

namespace space {

namespace detail {

const object_space make_contract_bytecode()
{
   object_space s;
   s.set_system( true );
   s.set_zone( zone::kernel );
   s.set_id( std::underlying_type_t< id >( id::contract_bytecode ) );
   return s;
}

const object_space make_contract_hash()
{
   object_space s;
   s.set_system( true );
   s.set_zone( zone::kernel );
   s.set_id( std::underlying_type_t< id >( id::contract_hash ) );
   return s;
}

const object_space make_system_call_dispatch()
{
   object_space s;
   s.set_system( true );
   s.set_zone( zone::kernel );
   s.set_id( std::underlying_type_t< id >( id::system_call_dispatch ) );
   return s;
}

const object_space make_meta()
{
   object_space s;
   s.set_system( true );
   s.set_zone( zone::kernel );
   s.set_id( std::underlying_type_t< id >( id::meta ) );
   return s;
}

const object_space make_transaction_nonce()
{
   object_space s;
   s.set_system( true );
   s.set_zone( zone::kernel );
   s.set_id( std::underlying_type_t< id >( id::transaction_nonce ) );
   return s;
}

} // detail

const object_space contract_bytecode()
{
   static auto s = detail::make_contract_bytecode();
   return s;
}

const object_space contract_hash()
{
   static auto s = detail::make_contract_hash();
   return s;
}

const object_space system_call_dispatch()
{
   static auto s = detail::make_system_call_dispatch();
   return s;
}

const object_space meta()
{
   static auto s = detail::make_meta();
   return s;
}

const object_space transaction_nonce()
{
   static auto s = detail::make_transaction_nonce();
   return s;
}

} // space

void assert_permissions( const apply_context& context, const object_space& space )
{
   auto privilege = context.get_privilege();
   auto caller = context.get_caller();
   if ( util::converter::to< std::vector< std::byte > >( space.zone() ) != caller )
   {
      if ( context.get_privilege() == privilege::kernel_mode )
      {
         KOINOS_ASSERT( space.system(), insufficient_privileges, "privileged code can only accessed system space" );
      }
      else
      {
         KOINOS_THROW( out_of_bounds, "contract attempted access of non-contract database space" );
      }
   }
}

} // koinos::chain::state
