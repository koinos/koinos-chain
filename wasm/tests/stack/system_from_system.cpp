
#include <koinos/system/system_calls.hpp>

using namespace std::string_literals;

const std::string stack_assertion_id = "\x00\xd5\x54\xbc\x09\x8a\xb2\xb0\x36\x6b\xbc\xe8\x78\x44\x1f\xa0\x2e\xe8\x10\x29\xe1\xaa\x0c\x28\x3f"s;

int main()
{
   auto args = koinos::system::get_contract_arguments();
   const auto [ caller, privilege ] = koinos::system::get_caller();

   if ( privilege != koinos::chain::privilege::kernel_mode )
   {
      koinos::system::print( "expected kernel mode, was user mode" );
      return 1;
   }

   koinos::system::call_contract( stack_assertion_id, 0, "\x01"s );

   return 0;
}
