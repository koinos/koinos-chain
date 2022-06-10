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
   std::string s = std::to_string( accumulator );
   r.mutable_object().set( (uint8_t*)s.data(), s.size() );

   koinos::system::exit( 0, r );
}
