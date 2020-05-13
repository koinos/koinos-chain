#pragma once
#include <cstdint>
#include <optional>
#include <map>

#include <koinos/exception.hpp>

namespace koinos::chain {

DECLARE_KOINOS_EXCEPTION( syscall_not_overridable );

// For any given system call, two slots are used. The first definition
// is considered overridable. The second system call slot is prefixed
// with an underscore to denote a private unoverridable implementation.

// When adding a system call slot, use the provided macro SYSCALL_SLOT
// to declare both a public and private implementation.

#define SYSCALL_SLOT(s) s, _##s

enum class syscall_slot : uint32_t
{
   SYSCALL_SLOT(register_syscall),
   SYSCALL_SLOT(verify_block_header),
   SYSCALL_SLOT(call_contract),

   SYSCALL_SLOT(prints),
   SYSCALL_SLOT(prints_l),
   SYSCALL_SLOT(printi),
   SYSCALL_SLOT(printui),
   SYSCALL_SLOT(printi128),
   SYSCALL_SLOT(printui128),
   SYSCALL_SLOT(printsf),
   SYSCALL_SLOT(printdf),
   SYSCALL_SLOT(printqf),
   SYSCALL_SLOT(printn),
   SYSCALL_SLOT(printhex),

   SYSCALL_SLOT(memset),
   SYSCALL_SLOT(memcmp),
   SYSCALL_SLOT(memmove),
   SYSCALL_SLOT(memcpy),

   SYSCALL_SLOT(current_receiver),
   SYSCALL_SLOT(action_data_size),
   SYSCALL_SLOT(read_action_data),

   SYSCALL_SLOT(eosio_assert),
   SYSCALL_SLOT(eosio_assert_message),
   SYSCALL_SLOT(eosio_assert_code),
   SYSCALL_SLOT(eosio_exit),
   SYSCALL_SLOT(abort),

   SYSCALL_SLOT(db_store_i64),
   SYSCALL_SLOT(db_update_i64),
   SYSCALL_SLOT(db_remove_i64),
   SYSCALL_SLOT(db_get_i64),
   SYSCALL_SLOT(db_next_i64),
   SYSCALL_SLOT(db_previous_i64),
   SYSCALL_SLOT(db_find_i64),
   SYSCALL_SLOT(db_lowerbound_i64),
   SYSCALL_SLOT(db_upperbound_i64),
   SYSCALL_SLOT(db_end_i64),

   SYSCALL_SLOT(db_idx64_store),
   SYSCALL_SLOT(db_idx64_update),
   SYSCALL_SLOT(db_idx64_remove),
   SYSCALL_SLOT(db_idx64_next),
   SYSCALL_SLOT(db_idx64_previous),
   SYSCALL_SLOT(db_idx64_find_primary),
   SYSCALL_SLOT(db_idx64_find_secondary),
   SYSCALL_SLOT(db_idx64_lowerbound),
   SYSCALL_SLOT(db_idx64_upperbound),
   SYSCALL_SLOT(db_idx64_end),

   SYSCALL_SLOT(db_idx128_store),
   SYSCALL_SLOT(db_idx128_update),
   SYSCALL_SLOT(db_idx128_remove),
   SYSCALL_SLOT(db_idx128_next),
   SYSCALL_SLOT(db_idx128_previous),
   SYSCALL_SLOT(db_idx128_find_primary),
   SYSCALL_SLOT(db_idx128_find_secondary),
   SYSCALL_SLOT(db_idx128_lowerbound),
   SYSCALL_SLOT(db_idx128_upperbound),
   SYSCALL_SLOT(db_idx128_end),

   SYSCALL_SLOT(db_idx256_store),
   SYSCALL_SLOT(db_idx256_update),
   SYSCALL_SLOT(db_idx256_remove),
   SYSCALL_SLOT(db_idx256_next),
   SYSCALL_SLOT(db_idx256_previous),
   SYSCALL_SLOT(db_idx256_find_primary),
   SYSCALL_SLOT(db_idx256_find_secondary),
   SYSCALL_SLOT(db_idx256_lowerbound),
   SYSCALL_SLOT(db_idx256_upperbound),
   SYSCALL_SLOT(db_idx256_end),

   SYSCALL_SLOT(db_idx_double_store),
   SYSCALL_SLOT(db_idx_double_update),
   SYSCALL_SLOT(db_idx_double_remove),
   SYSCALL_SLOT(db_idx_double_next),
   SYSCALL_SLOT(db_idx_double_previous),
   SYSCALL_SLOT(db_idx_double_find_primary),
   SYSCALL_SLOT(db_idx_double_find_secondary),
   SYSCALL_SLOT(db_idx_double_lowerbound),
   SYSCALL_SLOT(db_idx_double_upperbound),
   SYSCALL_SLOT(db_idx_double_end),

   SYSCALL_SLOT(db_idx_long_double_store),
   SYSCALL_SLOT(db_idx_long_double_update),
   SYSCALL_SLOT(db_idx_long_double_remove),
   SYSCALL_SLOT(db_idx_long_double_next),
   SYSCALL_SLOT(db_idx_long_double_previous),
   SYSCALL_SLOT(db_idx_long_double_find_primary),
   SYSCALL_SLOT(db_idx_long_double_find_secondary),
   SYSCALL_SLOT(db_idx_long_double_lowerbound),
   SYSCALL_SLOT(db_idx_long_double_upperbound),
   SYSCALL_SLOT(db_idx_long_double_end)
};

#undef SYSCALL_SLOT

using vm_code_ptr = int;

class syscall_table final
{
   public:
      void update();
      void register_syscall( syscall_slot s, vm_code_ptr v );

   private:
      std::map< syscall_slot, std::optional< vm_code_ptr > > syscall_mapping;
      std::map< syscall_slot, vm_code_ptr >                  pending_updates;

      bool overridable( syscall_slot s ) noexcept;
};

} // koinos::chain
