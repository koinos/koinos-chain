
#include <utility>

#include <stdint.h>
#include <string.h>

#include <koinos/chain/system_call_ids.h>

char prepend[] = "test: ";
#define PREPEND_LEN 6

extern "C" {
   uint32_t invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, char* arg_ptr, uint32_t arg_len );
   uint32_t invoke_thunk( uint32_t tid, char* ret_ptr, uint32_t ret_len, char* arg_ptr, uint32_t arg_len );
}

void log( char* msg )
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

   invoke_thunk( std::underlying_type_t< koinos::chain::system_call_id >( koinos::chain::system_call_id::log ), 0, 0, args, i + PREPEND_LEN + 2 );
}

int main()
{
   char message[65];
   memset( message, 0, 65 );
   invoke_system_call( std::underlying_type_t< koinos::chain::system_call_id >( koinos::chain::system_call_id::get_contract_arguments ), message, 63, 0, 0 );
   log( message + 4 );
}
