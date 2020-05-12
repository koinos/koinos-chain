#pragma once
#include <cstdint>
#include <optional>
#include <map>

#include <koinos/exception.hpp>

namespace koinos::kernel {

DECLARE_KOINOS_EXCEPTION( syscall_not_overridable );

using vm_code_ptr = int;

enum class syscall_slot : uint32_t
{
   register_syscall,
   verify_block_header,
   call_contract,

   prints,
   prints_l,
   printi,
   printui,
   printi128,
   printui128,
   printsf,
   printdf,
   printqf,
   printn,
   printhex,

   memset,
   memcmp,
   memmove,
   memcpy,

   current_receiver,
   action_data_size,
   read_action_data,

   eosio_assert,
   eosio_assert_message,
   eosio_assert_code,
   eosio_exit,
   abort,

   db_store_i64,
   db_update_i64,
   db_remove_i64,
   db_get_i64,
   db_next_i64,
   db_previous_i64,
   db_find_i64,
   db_lowerbound_i64,
   db_upperbound_i64,
   db_end_i64,

   db_idx64_store,
   db_idx64_update,
   db_idx64_remove,
   db_idx64_next,
   db_idx64_previous,
   db_idx64_find_primary,
   db_idx64_find_secondary,
   db_idx64_lowerbound,
   db_idx64_upperbound,
   db_idx64_end,

   db_idx128_store,
   db_idx128_update,
   db_idx128_remove,
   db_idx128_next,
   db_idx128_previous,
   db_idx128_find_primary,
   db_idx128_find_secondary,
   db_idx128_lowerbound,
   db_idx128_upperbound,
   db_idx128_end,

   db_idx256_store,
   db_idx256_update,
   db_idx256_remove,
   db_idx256_next,
   db_idx256_previous,
   db_idx256_find_primary,
   db_idx256_find_secondary,
   db_idx256_lowerbound,
   db_idx256_upperbound,
   db_idx256_end,

   db_idx_double_store,
   db_idx_double_update,
   db_idx_double_remove,
   db_idx_double_next,
   db_idx_double_previous,
   db_idx_double_find_primary,
   db_idx_double_find_secondary,
   db_idx_double_lowerbound,
   db_idx_double_upperbound,
   db_idx_double_end,

   db_idx_long_double_store,
   db_idx_long_double_update,
   db_idx_long_double_remove,
   db_idx_long_double_next,
   db_idx_long_double_previous,
   db_idx_long_double_find_primary,
   db_idx_long_double_find_secondary,
   db_idx_long_double_lowerbound,
   db_idx_long_double_upperbound,
   db_idx_long_double_end
};

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

} // koinos::kernel
