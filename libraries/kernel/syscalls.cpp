#include <koinos/kernel/syscalls.hpp>
#include <koinos/kernel/privilege.hpp>

namespace koinos::kernel {

bool syscall_table::overridable( syscall_slot s ) noexcept
{
   switch ( s )
   {
      case syscall_slot::register_syscall:
         return false;
         break;
      default:
         break;
   }
   return true;
}

void syscall_table::update()
{
   for ( auto& [ key, value ] : pending_updates )
      syscall_mapping[ key ] = value;

   pending_updates.clear();
}

void syscall_table::register_syscall( syscall_slot s, vm_code_ptr v )
{
   KOINOS_ASSERT(
      privilege::kernel_mode == privilege::kernel_mode,
      insufficient_privileges,
      "registering syscalls requires escalated privileges" );

   KOINOS_ASSERT(
      overridable( s ),
      syscall_not_overridable,
      "syscall cannot be overridden" );

   pending_updates[ s ] = v;
}

} // koinos::kernel
