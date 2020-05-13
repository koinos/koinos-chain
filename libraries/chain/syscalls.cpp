#include <koinos/chain/syscalls.hpp>
#include <koinos/chain/privilege.hpp>

namespace koinos::chain {

bool syscall_table::overridable( syscall_slot s ) noexcept
{
   return static_cast< uint32_t >( s ) % 2 == 0;
}

void syscall_table::update()
{
   for ( auto& [ key, value ] : pending_updates )
      syscall_mapping[ key ] = value;

   pending_updates.clear();
}

void syscall_table::register_syscall( syscall_slot s, vm_code_ptr v )
{
   #pragma message( "TODO: Change the implementation to check the privilege level of the current context" )
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

} // koinos::chain
