
#include <utility>
#include <stdint.h>

#include <koinos/chain/system_call_ids.h>

extern "C" {
   uint32_t invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, char* arg_ptr, uint32_t arg_len, uint32_t* bytes_written );
}

void log( char* msg, bool crash )
{
   char args[129];
   uint32_t bytes_written = 0;
   uint32_t *bytes_written_ptr;

   int i = 0;
   while( msg[i] && i < 127 )
   {
      args[i+2] = msg[i];
      i++;
   }
   args[0] = '\x0a';
   args[1] = (uint8_t)i;

   if( crash )
      bytes_written_ptr = (uint32_t*) 0xffffffff;
   else
      bytes_written_ptr = &bytes_written;

   invoke_system_call( std::underlying_type_t< koinos::chain::system_call_id >( koinos::chain::system_call_id::log ), 0, 0, args, i + 2, bytes_written_ptr );
}

int main()
{
   log("does not crash", false);
   log("definitely crash", true);
   return 0;
}
