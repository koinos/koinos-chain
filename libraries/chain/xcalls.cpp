
#include <koinos/chain/system_call_utils.hpp>
#include <koinos/chain/thunks.hpp>
#include <koinos/chain/xcalls.hpp>

#include <koinos/pack/classes.hpp>
#include <koinos/pack/rt/binary.hpp>

namespace koinos::chain {

using koinos::protocol::thunk_id_type;
using koinos::protocol::xcall_target;

/*
 * This is a list of syscalls registered at genesis.
 *
 * For initial Koinos development, this declaration should match REGISTER_THUNKS.
 * However, as soon as a new thunk is added as an in band upgrade, it should be
 * added only to REGISTER_THUNKS, not here. The registration of that thunk as a
 * syscall happens as an in band upgrade.
 */
DEFAULT_SYS_CALLS(
   (prints)

   (verify_block_header)

   (apply_block)
   (apply_transaction)
   (apply_upload_contract_operation)
   (apply_execute_contract_operation)

   (db_put_object)
   (db_get_object)
   (db_get_next_object)
   (db_get_prev_object)
)

} // koinos::chain
