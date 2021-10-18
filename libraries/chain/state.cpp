#include <koinos/chain/state.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/log.hpp>

namespace koinos::chain::state {

namespace space {

const object_space contract()
{
   object_space s;
   s.set_system( true );
   s.set_zone( zone::kernel );
   s.set_id( std::underlying_type_t< id >( id::contract ) );
   return s;
}

const object_space system_call_dispatch()
{
   object_space s;
   s.set_system( true );
   s.set_zone( zone::kernel );
   s.set_id( std::underlying_type_t< id >( id::system_call_dispatch ) );
   return s;
}

const object_space meta()
{
   object_space s;
   s.set_system( true );
   s.set_zone( zone::kernel );
   s.set_id( std::underlying_type_t< id >( id::meta ) );
   return s;
}

} // space

namespace key {

std::string transaction_nonce( const std::string& payer )
{
   auto payer_key = converter::to< uint160_t >( payer );
   auto trx_nonce_key = converter::to< uint64_t >( crypto::hash( crypto::multicodec::ripemd_160, std::string( "object_key::nonce" ) ).digest() );
   return converter::as< std::string >( payer_key, trx_nonce_key );
}

} // key

void assert_permissions( const apply_context& context, const object_space& space )
{
   auto privilege = context.get_privilege();
   auto caller = context.get_caller();
   if ( converter::to< std::vector< std::byte > >( space.zone() ) != caller )
   {
      if ( context.get_privilege() == privilege::kernel_mode )
      {
         KOINOS_ASSERT( space.system(), insufficient_privileges, "privileged code can only accessed system space" );
      }
      else
      {
         LOG(info) << to_hex( converter::as< std::string >( space ) ) << ", " << to_hex( converter::as< std::string >( caller ) );
         KOINOS_THROW( out_of_bounds, "contract attempted access of non-contract database space" );
      }
   }
}

void assert_transaction_nonce( apply_context& ctx, const std::string& payer, uint64_t nonce )
{
   auto account_nonce = system_call::get_account_nonce( ctx, payer );
   KOINOS_ASSERT(
      account_nonce == nonce,
      chain::chain_exception,
      "mismatching transaction nonce - trx nonce: ${d}, expected: ${e}", ("d", nonce)("e", account_nonce)
   );
}

void update_transaction_nonce( apply_context& ctx, const std::string& payer, uint64_t nonce )
{
   system_call::put_object( ctx, state::space::meta(), state::key::transaction_nonce( payer ), converter::as< std::string >( nonce ) );
}

} // koinos::chain::state
