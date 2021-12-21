#include <utility>

#include <stdint.h>
#include <string.h>

#include <koinos/protocol/system_call_ids.h>

extern "C" {
   uint32_t invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, char* arg_ptr, uint32_t arg_len );
}

int main()
{
   char message[64];
   memset( message, 0, 64 );
   invoke_system_call( std::underlying_type_t< koinos::protocol::system_call_id >( koinos::protocol::system_call_id::get_contract_arguments ), message, 64, 0, 0 );
   invoke_system_call( std::underlying_type_t< koinos::protocol::system_call_id >( koinos::protocol::system_call_id::set_contract_result ), 0, 0, message, 64 );
   return 0;
}
