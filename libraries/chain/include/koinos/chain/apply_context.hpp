#pragma once

#include <koinos/chain/privilege.hpp>

#include <koinos/statedb/statedb.hpp>

#include <string>

namespace koinos { namespace chain {

using koinos::statedb::state_node_ptr;

class system_call_table;
class apply_context
{
   public:
      apply_context( system_call_table& sct );

   /// Console methods:
      void console_append( const std::string& val ) {
         pending_console_output += val;
      }
      std::string get_pending_console_output() { return pending_console_output; }

      void set_state_node( state_node_ptr );
      state_node_ptr get_state_node() const;
      void clear_state_node();

   /// Fields:
   public:
      system_call_table&            syscalls;
      privilege                     privilege_level = privilege::user_mode;

   private:
      std::string                   pending_console_output;
      state_node_ptr                current_state_node;
};

} } // koinos::chain
