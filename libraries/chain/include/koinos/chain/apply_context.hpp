#pragma once

#include <koinos/chain/privilege.hpp>
#include <koinos/statedb/statedb.hpp>
#include <koinos/pack/classes.hpp>
#include <koinos/crypto/elliptic.hpp>

#include <deque>
#include <optional>
#include <string>

#define STACK_LIMIT 256

namespace koinos::chain {

using boost::container::flat_set;
using koinos::statedb::state_node_ptr;

KOINOS_DECLARE_EXCEPTION( stack_exception );
KOINOS_DECLARE_EXCEPTION( stack_overflow );

struct stack_frame
{
   account_type  call;
   variable_blob call_args;
   variable_blob call_return;
};

class apply_context
{
   public:
      apply_context() = default;

      /// Console methods:
      void console_append( const std::string& val );
      std::string get_pending_console_output();

      void set_state_node( state_node_ptr );
      state_node_ptr get_state_node() const;
      void clear_state_node();

      void set_block( const protocol::block& );
      const protocol::block& get_block() const;
      void clear_block();

      void set_transaction( const opaque< protocol::transaction >& );
      const opaque< protocol::transaction >& get_transaction() const;
      void clear_transaction();

      void set_contract_call_args( const variable_blob& args );
      const variable_blob& get_contract_call_args() const;

      variable_blob get_contract_return() const;
      void set_contract_return( const variable_blob& ret );

      /**
       * For now, authority lives on the context.
       * This should be moved, made generic, or otherwise re-architected.
       */
      void set_key_authority( const crypto::public_key& key );
      void clear_authority();

      void push_frame( stack_frame&& frame );
      stack_frame pop_frame();

      const account_type& get_caller()const;

   /// Fields:
   public:
      privilege                     privilege_level = privilege::user_mode;

   private:
      state_node_ptr                      _current_state_node;
      std::string                         _pending_console_output;
      std::optional< crypto::public_key > _key_auth;

      std::vector< stack_frame >          _stack;

      const protocol::block*                 _block = nullptr;
      const opaque< protocol::transaction >* _trx = nullptr;
};

} // koinos::chain
