#include <stdint.h>
#include <string.h>

#define KOINOS_SYSTEM_CALL_ID_get_contract_args 15
#define KOINOS_THUNK_ID_prints 1

char prepend[] = "test: ";
#define PREPEND_LEN 6

extern "C" {
   uint32_t invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, char* arg_ptr, uint32_t arg_len );
   uint32_t invoke_thunk( uint32_t tid, char* ret_ptr, uint32_t ret_len, char* arg_ptr, uint32_t arg_len );
}

void prints( char* msg )
{
   char args[129];

   int i = 0;

   args[0] = '\x0a';

   while( i < PREPEND_LEN )
   {
      args[i+2] = prepend[i];
      i++;
   }

   i = 0;

   while( msg[i] && i < 129 )
   {
      args[i + PREPEND_LEN + 2] = msg[i];
      i++;
   }

   args[1] = (uint8_t)(i + PREPEND_LEN);

   invoke_thunk( KOINOS_THUNK_ID_prints, 0, 0, args, i + PREPEND_LEN + 2 );
}

int main()
{
   char message[65];
   memset( message, 0, 65 );
   invoke_system_call( KOINOS_SYSTEM_CALL_ID_get_contract_args, message, 63, 0, 0 );
   prints( message + 4 );
}
