#include <stdint.h>

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

   invoke_thunk( 0, 0, 0, args, i + PREPEND_LEN + 1 );
}

__attribute__( (visibility("default")) )
void apply( uint64_t a, uint64_t b, uint64_t c )
{
   char message[65];
   invoke_system_call( 14, message, 63, 0, 0 );
   message[ message[0] + 1 ] = 0;
   prints( message + 2 );
}
