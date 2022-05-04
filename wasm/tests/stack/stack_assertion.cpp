
#include <koinos/system/system_calls.hpp>

int main()
{
   auto args = koinos::system::get_arguments();
   const auto [ caller, privilege ] = koinos::system::get_caller();

   // zero is user mode, non-zero is kernel mode
   if ( ( args.c_str()[0] == 0 ) != ( privilege == koinos::chain::privilege::user_mode ) )
   {
      if ( args.c_str()[0] == 0 )
         koinos::system::revert( "expected user mode, was kernel mode" );
      else
         koinos::system::revert( "expected kernel mode, was user mode" );
   }
}
