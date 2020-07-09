#pragma once

#include <koinos/chain/privilege.hpp>
#include <koinos/statedb/statedb.hpp>
#include <koinos/pack/rt/basetypes.hpp>
#include <koinos/crypto/elliptic.hpp>

#include <optional>
#include <string>

namespace koinos::chain {

using boost::container::flat_set;
using koinos::statedb::state_node_ptr;

class apply_context
{
   public:
      apply_context() = default;

      /// Console methods:
      void console_append( const std::string& val )
      {
         pending_console_output += val;
      }

      std::string get_pending_console_output()
      {
         std::string buf = pending_console_output;
         pending_console_output.clear();
         return buf;
      }

      void set_state_node( state_node_ptr );
      state_node_ptr get_state_node() const;
      void clear_state_node();

      void set_contract_call_args( const types::variable_blob& args );
      const types::variable_blob& get_contract_call_args();

      types::variable_blob get_contract_return();
      void set_contract_return( const types::variable_blob& ret );

      /**
       * For now, authority lives on the context.
       * This should be moved, made generic, or otherwise re-architected.
       */
      void set_key_authority( const crypto::public_key& key );
      void clear_authority();

   /// Fields:
   public:
      privilege                     privilege_level = privilege::user_mode;

   private:
      std::string                   pending_console_output;
      state_node_ptr                current_state_node;
      types::variable_blob          contract_call_args;
      types::variable_blob          contract_return;
      std::optional< crypto::public_key > key_auth;
};

} // koinos::chain
