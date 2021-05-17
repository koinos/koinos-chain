
#include <stdint.h>

#define KOINOS_SYSTEM_CALL_ID_prints 0x9b229941

extern "C" {
   void invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, char* arg_ptr, uint32_t arg_len );
}

void prints( char* msg )
{
   char args[129];

   int i = 0;
   while( msg[i] && i < 127 )
   {
      args[i+1] = msg[i];
      i++;
   }
   args[0] = (uint8_t)i;

   invoke_system_call( KOINOS_SYSTEM_CALL_ID_prints, 0, 0, args, i + 1 );
}

int main()
{
   prints( "Greetings from koinos vm" );
   return 0;
}
