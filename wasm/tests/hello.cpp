
#include <utility>
#include <stdint.h>

#include <koinos/protocol/system_call_ids.h>

extern "C" {
   uint32_t invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, char* arg_ptr, uint32_t arg_len );
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
   args[0] = '\x0a';
   args[1] = (uint8_t)i;

   invoke_system_call( std::underlying_type_t< koinos::protocol::system_call_id >( koinos::protocol::system_call_id::prints ), 0, 0, args, i + 2 );
}

int main()
{
   prints( "Greetings from koinos vm" );
   return 0;
}
