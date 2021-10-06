#include <stdint.h>
#include <string.h>

#define KOINOS_SYSTEM_CALL_ID_get_contract_args 15
#define KOINOS_SYSTEM_CALL_ID_set_contract_return 16

extern "C" {
   uint32_t invoke_system_call( uint32_t sid, char* ret_ptr, uint32_t ret_len, char* arg_ptr, uint32_t arg_len );
}

int main()
{
   char message[64];
   memset( message, 0, 64 );
   invoke_system_call( KOINOS_SYSTEM_CALL_ID_get_contract_args, message, 64, 0, 0 );
   invoke_system_call( KOINOS_SYSTEM_CALL_ID_set_contract_return, 0, 0, message, 64 );
   return 0;
}
