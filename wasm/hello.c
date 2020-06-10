
#include <stdint.h>

void prints( const char* str );

__attribute__( (visibility("default")) )
void apply( uint64_t a, uint64_t b, uint64_t c )
{
   prints( "Greetings from koinos vm" );
}
