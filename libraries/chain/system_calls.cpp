
#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/thunk_utils.hpp>
#include <koinos/chain/thunks.hpp>

#include <koinos/pack/classes.hpp>
#include <koinos/pack/rt/binary.hpp>

namespace koinos::chain {

using namespace koinos::types::system;
using namespace koinos::types::thunks;

/*
 * This is a list of syscalls registered at genesis.
 *
 * For initial Koinos development, this declaration should match REGISTER_THUNKS.
 * However, as soon as a new thunk is added as an in band upgrade, it should be
 * added only to REGISTER_THUNKS, not here. The registration of that thunk as a
 * syscall happens as an in band upgrade.
 */
DEFAULT_SYSTEM_CALLS(
   (prints)
   (exit_contract)

   (verify_block_header)

   (apply_block)
   (apply_transaction)
   (apply_reserved_operation)
   (apply_upload_contract_operation)
   (apply_execute_contract_operation)
   (apply_set_system_call_operation)

   (db_put_object)
   (db_get_object)
   (db_get_next_object)
   (db_get_prev_object)

   (execute_contract)

   (get_contract_args_size)
   (get_contract_args)
   (set_contract_return)
)

} // koinos::chain
