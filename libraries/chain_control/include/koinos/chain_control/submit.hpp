#pragma once

#include <koinos/pack/classes.hpp>

//
// This header file is a bit of a hack
// The C++ code generator doesn't actually support namespaces
// so we are stuck putting everything in koinos::protocol and then
// copying it to the correct namespace with using
//

namespace koinos { namespace chain_control {

using koinos::protocol::submit_reserved;
using koinos::protocol::submit_block;
using koinos::protocol::submit_transaction;
using koinos::protocol::submit_query;
using koinos::protocol::submit_item;

using koinos::protocol::submit_return;
using koinos::protocol::submit_return_reserved;
using koinos::protocol::submit_return_block;
using koinos::protocol::submit_return_transaction;
using koinos::protocol::submit_return_query;
using koinos::protocol::submit_return_error;

} } // koinos::chain_control
