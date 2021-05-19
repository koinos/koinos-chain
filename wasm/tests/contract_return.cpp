#include <stdint.h>

#define KOINOS_SYSTEM_CALL_ID_get_contract_args 0x9fbba198
#define KOINOS_SYSTEM_CALL_ID_set_contract_return 0x9f49cdea

extern "C" {
   void invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, char* arg_ptr, uint32_t arg_len );
}

void prints( char* msg )
{
   char args[129];

   int i = 0;
   while( msg[i] && i < 127 )
   {
      args[i+2] = msg[i];
      i++;
   }
   args[0] = (uint8_t)(i+1);
   args[1] = (uint8_t)(i);

   invoke_system_call( KOINOS_SYSTEM_CALL_ID_set_contract_return, 0, 0, args, i + 2 );
}

int main()
{
   char message[64];
   invoke_system_call( KOINOS_SYSTEM_CALL_ID_get_contract_args, message, 63, 0, 0 );
   message[ message[0] + 1 ] = 0;
   prints( message + 2 );
   return 0;
}
