#include <stdint.h>

#define KOINOS_SYSTEM_CALL_ID_get_contract_args 0x9fbba198
#define KOINOS_THUNK_ID_prints 0x8f6df54d

char prepend[] = "test: ";
#define PREPEND_LEN sizeof(prepend) - 1

void invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, char* arg_ptr, uint32_t arg_len );
void invoke_thunk( uint32_t tid, char* ret_ptr, uint32_t ret_len, char* arg_ptr, uint32_t arg_len );

void prints( char* msg )
{
   char args[129];

   int i = 0;

   while( i < PREPEND_LEN )
   {
      args[i+1] = prepend[i];
      i++;
   }

   i = 0;

   while( msg[i] && i < 129 - PREPEND_LEN - 1 )
   {
      args[i+PREPEND_LEN+1] = msg[i];
      i++;
   }
   args[0] = (uint8_t)(i + PREPEND_LEN);

   invoke_thunk( KOINOS_THUNK_ID_prints, 0, 0, args, i + PREPEND_LEN + 1 );
}

__attribute__( (visibility("default")) )
void _start()
{
   char message[65];
   invoke_system_call( KOINOS_SYSTEM_CALL_ID_get_contract_args, message, 63, 0, 0 );
   message[ message[0] + 1 ] = 0;
   prints( message + 2 );
}
