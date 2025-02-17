#include <koinos/chain/rectify.hpp>

#include <koinos/util/base64.hpp>
#include <koinos/util/hex.hpp>

#include <google/protobuf/util/json_util.h>

namespace koinos::chain {

void maybe_rectify_state( execution_context& ctx, const protocol::block& block, protocol::block_receipt& block_receipt )
{
  if(
    block.header().height() == 9'180'357 &&
    block.id() == util::from_hex< std::string >( "0x1220f66b60a65c8614eda8b70a03df13a6f53a2089111dc6eed1a286d879d22e84b4" ) )
  {
    block_receipt.set_compute_bandwidth_used( 17'631'052 );
    block_receipt.set_compute_bandwidth_charged( 17'557'631 );
    block_receipt.mutable_transaction_receipts( 0 )->set_compute_bandwidth_used( 14'582'049 );
    block_receipt.mutable_transaction_receipts( 0 )->set_rc_used( 423'344'309 );

    auto block_node = ctx.get_state_node();

    object_space space;
    space.set_zone( util::from_base64< std::string >( "ALJp6C6zICjLRQ8DEj48TS+Rp8fr9OW8fA==" ) );
    space.set_system(true);

    auto* state_entry = block_receipt.mutable_state_delta_entries( 5 );
    state_entry->mutable_object_space()->set_zone( space.zone() );
    state_entry->mutable_object_space()->set_system( space.system() );
    state_entry->set_key( util::from_base64< std::string >( "bWFya2V0cw==" ) );
    state_entry->set_value( util::from_base64< std::string >( "Cg4IgdDQhB8YsLUCIICAIBIPCJThs6XNARiAgBAggIBAGhMI0beCnLHVAhjgwrUbIODNi4kB" ) );

    block_node->put_object( space, state_entry->key(), &state_entry->value() );

    space.set_zone( util::from_base64< std::string >( "AC4z/RqpB7IkzpzmyUIokB0oOgLalW2nkQ==" ) );
    space.set_id( 1 );
    space.set_system( true );

    state_entry = block_receipt.mutable_state_delta_entries( 63 );
    state_entry->mutable_object_space()->set_zone( space.zone() );
    state_entry->mutable_object_space()->set_id( space.id() );
    state_entry->mutable_object_space()->set_system( space.system() );
    state_entry->set_key( util::from_base64< std::string >( "ANAlFhI9x76hhWhfte5LvQ6NfFoHakTMtQ==" ) );
    state_entry->set_value( util::from_base64< std::string >( "CICOmsf2EhDLnKv99BIY2IHul64x" ) );
    auto* trx_state_entry = block_receipt.mutable_transaction_receipts( 0 )->mutable_state_delta_entries( 62 );
    *(trx_state_entry->mutable_object_space()) = state_entry->object_space();
    trx_state_entry->set_key( state_entry->key() );
    trx_state_entry->set_value( state_entry->value() );

    block_node->put_object( space, state_entry->key(), &state_entry->value() );
  }
}

} // koinos::chain