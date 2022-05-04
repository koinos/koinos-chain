
#include <koinos/system/system_calls.hpp>

int main()
{
   koinos::system::event( std::string(), koinos::chain::event_result(), std::vector< std::string >() );
   koinos::system::exit( 0 );

   return 0;
}
