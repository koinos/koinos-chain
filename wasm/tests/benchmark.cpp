#include <cstdint>
#include <string>
#include <koinos/system/system_calls.hpp>

int main()
{
   uint64_t accumulator = 0;
   for ( uint64_t i = 0; i < 10'000; i++ )
   {
         if ( i % 3 )
            accumulator += 3;
         else if ( i % 2 )
            accumulator += 2;
         else
            accumulator += 1;
   }

   koinos::system::result r;
   r.set_code( (uint32_t) accumulator );

   koinos::system::exit( r );

   return 0;
}
