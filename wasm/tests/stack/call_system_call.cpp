
#include <koinos/system/system_calls.hpp>

int main()
{
   koinos::system::log( "foobar" );
   koinos::system::exit( 0 );

   return 0;
}
