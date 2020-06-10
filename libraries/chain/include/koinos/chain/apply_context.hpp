#pragma once

#include <koinos/chain/privilege.hpp>
#include <string>

namespace koinos { namespace chain {
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

   /// Fields:
   public:
      system_call_table&            syscalls;
      privilege                     privilege_level = privilege::user_mode;

   private:
      std::string                   pending_console_output;
};

} } // koinos::chain
