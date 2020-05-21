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

} } // koinos::chain_control
