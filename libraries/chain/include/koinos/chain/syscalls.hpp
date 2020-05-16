#pragma once
#include <cstdint>
#include <optional>
#include <map>
#include <vector>

#include <koinos/exception.hpp>
#include <koinos/chain/wasm/common.hpp>
#include <koinos/chain/types.hpp>

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

struct syscall_bundle
{
   std::vector< uint8_t > wasm_bytes;
   name action;
};

class syscall_table final
{
   public:
      void update();
      void set_syscall( syscall_slot s, syscall_bundle v );
      std::optional< syscall_bundle > get_syscall( syscall_slot s );

   private:
      std::map< syscall_slot, syscall_bundle > syscall_map;
      std::map< syscall_slot, syscall_bundle > pending_updates;

      bool overridable( syscall_slot s ) noexcept;
};

// When defining a new system call, we have essentially two different implementations.
// One of the implementations is considered upgradeable and can be overridden with
// VM code. The other implementation is prefixed with an underscore and cannot be
// overridden and is the default implement if no override is provided.

// Use the macro SYSCALL_DECLARE to simultaneously declare both public and private versions.

#define SYSCALL_DECLARE(return_type, name, ...) \
   return_type name(__VA_ARGS__);               \
   return_type _##name(__VA_ARGS__)

struct system_api final
{
   system_api( apply_context& ctx );
   apply_context& context;

   SYSCALL_DECLARE( void, abort );
   SYSCALL_DECLARE( void, eosio_assert, bool condition, null_terminated_ptr msg );
   SYSCALL_DECLARE( void, eosio_assert_message, bool, array_ptr< const char > msg, uint32_t len );
   SYSCALL_DECLARE( void, eosio_assert_code, bool condition, uint64_t error_code );
   SYSCALL_DECLARE( void, eosio_exit, int32_t code );

   SYSCALL_DECLARE( int, read_action_data, array_ptr<char> memory, uint32_t buffer_size );
   SYSCALL_DECLARE( int, action_data_size );
   SYSCALL_DECLARE( name, current_receiver );

   SYSCALL_DECLARE( char*, memcpy, array_ptr< char > dest, array_ptr< const char > src, uint32_t length );
   SYSCALL_DECLARE( char*, memmove, array_ptr< char > dest, array_ptr< const char > src, uint32_t length );
   SYSCALL_DECLARE( int, memcmp, array_ptr< const char > dest, array_ptr< const char > src, uint32_t length );
   SYSCALL_DECLARE( char*, memset, array_ptr<char> dest, int value, uint32_t length );

   SYSCALL_DECLARE( void, prints, null_terminated_ptr str );
   SYSCALL_DECLARE( void, prints_l, array_ptr< const char > str, uint32_t str_len );
   SYSCALL_DECLARE( void, printi, int64_t val );
   SYSCALL_DECLARE( void, printui, uint64_t val );
   SYSCALL_DECLARE( void, printi128, const __int128& val );
   SYSCALL_DECLARE( void, printui128, const unsigned __int128& val );
   SYSCALL_DECLARE( void, printsf, float val );
   SYSCALL_DECLARE( void, printdf, double val );
   SYSCALL_DECLARE( void, printqf, const float128_t& val );
   SYSCALL_DECLARE( void, printn, name val );
   SYSCALL_DECLARE( void, printhex, array_ptr< const char > data, uint32_t data_len );

   SYSCALL_DECLARE( int, db_store_i64, uint64_t scope, uint64_t table, uint64_t payer, uint64_t id, array_ptr< const char > buffer, uint32_t buffer_size );
   SYSCALL_DECLARE( void, db_update_i64, int itr, uint64_t payer, array_ptr< const char > buffer, uint32_t buffer_size );
   SYSCALL_DECLARE( void, db_remove_i64, int itr );
   SYSCALL_DECLARE( int, db_get_i64, int itr, array_ptr< char > buffer, uint32_t buffer_size );
   SYSCALL_DECLARE( int, db_next_i64, int itr, uint64_t& primary );
   SYSCALL_DECLARE( int, db_previous_i64, int itr, uint64_t& primary );
   SYSCALL_DECLARE( int, db_find_i64, uint64_t code, uint64_t scope, uint64_t table, uint64_t id );
   SYSCALL_DECLARE( int, db_lowerbound_i64, uint64_t code, uint64_t scope, uint64_t table, uint64_t id );
   SYSCALL_DECLARE( int, db_upperbound_i64, uint64_t code, uint64_t scope, uint64_t table, uint64_t id );
   SYSCALL_DECLARE( int, db_end_i64, uint64_t code, uint64_t scope, uint64_t table );

   SYSCALL_DECLARE( int, db_idx64_store, uint64_t scope, uint64_t table, uint64_t payer, uint64_t id, const uint64_t& secondary );
   SYSCALL_DECLARE( void, db_idx64_update, int iterator, uint64_t payer, const uint64_t& secondary );
   SYSCALL_DECLARE( void, db_idx64_remove, int iterator );
   SYSCALL_DECLARE( int, db_idx64_find_secondary, uint64_t code, uint64_t scope, uint64_t table, const uint64_t& secondary, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx64_find_primary, uint64_t code, uint64_t scope, uint64_t table, uint64_t& secondary, uint64_t primary );
   SYSCALL_DECLARE( int, db_idx64_lowerbound, uint64_t code, uint64_t scope, uint64_t table, uint64_t& secondary, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx64_upperbound, uint64_t code, uint64_t scope, uint64_t table, uint64_t& secondary, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx64_end, uint64_t code, uint64_t scope, uint64_t table );
   SYSCALL_DECLARE( int, db_idx64_next, int iterator, uint64_t& primary  );
   SYSCALL_DECLARE( int, db_idx64_previous, int iterator, uint64_t& primary );

   SYSCALL_DECLARE( int, db_idx128_store, uint64_t scope, uint64_t table, uint64_t payer, uint64_t id, const uint128_t& secondary );
   SYSCALL_DECLARE( void, db_idx128_update, int iterator, uint64_t payer, const uint128_t& secondary );
   SYSCALL_DECLARE( void, db_idx128_remove, int iterator );
   SYSCALL_DECLARE( int, db_idx128_find_secondary, uint64_t code, uint64_t scope, uint64_t table, const uint128_t& secondary, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx128_find_primary, uint64_t code, uint64_t scope, uint64_t table, uint128_t& secondary, uint64_t primary );
   SYSCALL_DECLARE( int, db_idx128_lowerbound, uint64_t code, uint64_t scope, uint64_t table, uint128_t& secondary, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx128_upperbound, uint64_t code, uint64_t scope, uint64_t table, uint128_t& secondary, uint64_t& primary );;
   SYSCALL_DECLARE( int, db_idx128_end, uint64_t code, uint64_t scope, uint64_t table );
   SYSCALL_DECLARE( int, db_idx128_next, int iterator, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx128_previous, int iterator, uint64_t& primary );

   SYSCALL_DECLARE( int, db_idx256_store, uint64_t scope, uint64_t table, uint64_t payer, uint64_t id, array_ptr< const uint128_t > data, uint32_t data_len );
   SYSCALL_DECLARE( void, db_idx256_update, int iterator, uint64_t payer, array_ptr< const uint128_t > data, uint32_t data_len );
   SYSCALL_DECLARE( void, db_idx256_remove, int iterator );
   SYSCALL_DECLARE( int, db_idx256_find_secondary, uint64_t code, uint64_t scope, uint64_t table, array_ptr< const uint128_t > data, uint32_t data_len, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx256_find_primary, uint64_t code, uint64_t scope, uint64_t table, array_ptr< uint128_t > data, uint32_t data_len, uint64_t primary );
   SYSCALL_DECLARE( int, db_idx256_lowerbound, uint64_t code, uint64_t scope, uint64_t table, array_ptr< uint128_t > data, uint32_t data_len, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx256_upperbound, uint64_t code, uint64_t scope, uint64_t table, array_ptr< uint128_t > data, uint32_t data_len, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx256_end, uint64_t code, uint64_t scope, uint64_t table );
   SYSCALL_DECLARE( int, db_idx256_next, int iterator, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx256_previous, int iterator, uint64_t& primary );

   SYSCALL_DECLARE( int, db_idx_double_store, uint64_t scope, uint64_t table, uint64_t payer, uint64_t id, const float64_t& secondary );
   SYSCALL_DECLARE( void, db_idx_double_update, int iterator, uint64_t payer, const float64_t& secondary );
   SYSCALL_DECLARE( void, db_idx_double_remove, int iterator );
   SYSCALL_DECLARE( int, db_idx_double_find_secondary, uint64_t code, uint64_t scope, uint64_t table, const float64_t& secondary, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx_double_find_primary, uint64_t code, uint64_t scope, uint64_t table, float64_t& secondary, uint64_t primary );
   SYSCALL_DECLARE( int, db_idx_double_lowerbound, uint64_t code, uint64_t scope, uint64_t table, float64_t& secondary, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx_double_upperbound, uint64_t code, uint64_t scope, uint64_t table, float64_t& secondary, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx_double_end, uint64_t code, uint64_t scope, uint64_t table );
   SYSCALL_DECLARE( int, db_idx_double_next, int iterator, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx_double_previous, int iterator, uint64_t& primary );

   SYSCALL_DECLARE( int, db_idx_long_double_store, uint64_t scope, uint64_t table, uint64_t payer, uint64_t id, const float128_t& secondary );
   SYSCALL_DECLARE( void, db_idx_long_double_update, int iterator, uint64_t payer, const float128_t& secondary );
   SYSCALL_DECLARE( void, db_idx_long_double_remove, int iterator );
   SYSCALL_DECLARE( int, db_idx_long_double_find_secondary, uint64_t code, uint64_t scope, uint64_t table, const float128_t& secondary, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx_long_double_find_primary, uint64_t code, uint64_t scope, uint64_t table, float128_t& secondary, uint64_t primary );
   SYSCALL_DECLARE( int, db_idx_long_double_lowerbound, uint64_t code, uint64_t scope, uint64_t table, float128_t& secondary, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx_long_double_upperbound, uint64_t code, uint64_t scope, uint64_t table, float128_t& secondary, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx_long_double_end, uint64_t code, uint64_t scope, uint64_t table );
   SYSCALL_DECLARE( int, db_idx_long_double_next, int iterator, uint64_t& primary );
   SYSCALL_DECLARE( int, db_idx_long_double_previous, int iterator, uint64_t& primary );
};

#undef DECLARE_SYSCALL

} // koinos::chain
