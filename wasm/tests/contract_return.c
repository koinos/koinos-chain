#include <stdint.h>

#define KOINOS_SYSTEM_CALL_ID_get_contract_args 0x8e189d86
#define KOINOS_SYSTEM_CALL_ID_set_contract_return 0x86b86275

void invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, char* arg_ptr, uint32_t arg_len );

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

__attribute__( (visibility("default")) )
void apply( uint64_t a, uint64_t b, uint64_t c )
{
   char message[64];
   invoke_system_call( KOINOS_SYSTEM_CALL_ID_get_contract_args, message, 63, 0, 0 );
   message[ message[0] + 1 ] = 0;
   prints( message + 2 );
}
