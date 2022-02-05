
#include <utility>
#include <stdint.h>

#include <koinos/chain/system_call_ids.h>

extern "C" {
   uint32_t invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, char* arg_ptr, uint32_t arg_len );
}

void log( char* msg )
{
   char args[129];

   int i = 0;
   while( msg[i] && i < 127 )
   {
      args[i+2] = msg[i];
      i++;
   }
   args[0] = '\x0a';
   args[1] = (uint8_t)i;

   invoke_system_call( std::underlying_type_t< koinos::chain::system_call_id >( koinos::chain::system_call_id::log ), 0, 0, args, i + 2 );
}

int main()
{
   log( "Greetings from koinos vm" );
   return 0;
}
