#pragma once

#include <koinos/chain/execution_context.hpp>

#include <koinos/protocol/protocol.pb.h>

namespace koinos::chain {

void maybe_rectify_state( execution_context&, const protocol::block&, protocol::block_receipt& );

} // koinos::chain