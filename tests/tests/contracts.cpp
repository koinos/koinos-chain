#include <koinos/tests/contracts.hpp>

#include <koinos/tests/wasm/authorize.hpp>
#include <koinos/tests/wasm/benchmark.hpp>
#include <koinos/tests/wasm/contract_return.hpp>
#include <koinos/tests/wasm/db_write.hpp>
#include <koinos/tests/wasm/empty_contract.hpp>
#include <koinos/tests/wasm/forever.hpp>
#include <koinos/tests/wasm/hello.hpp>
#include <koinos/tests/wasm/koin.hpp>
#include <koinos/tests/wasm/syscall_override.hpp>

#include <koinos/tests/wasm/stack/call_contract.hpp>
#include <koinos/tests/wasm/stack/call_system_call.hpp>
#include <koinos/tests/wasm/stack/call_system_call2.hpp>
#include <koinos/tests/wasm/stack/stack_assertion.hpp>
#include <koinos/tests/wasm/stack/system_from_system.hpp>
#include <koinos/tests/wasm/stack/system_from_user.hpp>
#include <koinos/tests/wasm/stack/user_from_system.hpp>
#include <koinos/tests/wasm/stack/user_from_user.hpp>

#define KOINOS_DEFINE_GET_WASM( contract_name ) \
const std::string& get_ ## contract_name ## _wasm () \
{ \
   static std::string wasm( (const char*) contract_name ## _wasm, contract_name ## _wasm_len ); \
   return wasm; \
}

KOINOS_DEFINE_GET_WASM( authorize )
KOINOS_DEFINE_GET_WASM( benchmark )
KOINOS_DEFINE_GET_WASM( contract_return )
KOINOS_DEFINE_GET_WASM( db_write )
KOINOS_DEFINE_GET_WASM( empty_contract )
KOINOS_DEFINE_GET_WASM( forever )
KOINOS_DEFINE_GET_WASM( hello )
KOINOS_DEFINE_GET_WASM( koin )
KOINOS_DEFINE_GET_WASM( syscall_override )
KOINOS_DEFINE_GET_WASM( call_contract )
KOINOS_DEFINE_GET_WASM( call_system_call )
KOINOS_DEFINE_GET_WASM( call_system_call2 )
KOINOS_DEFINE_GET_WASM( stack_assertion )
KOINOS_DEFINE_GET_WASM( system_from_system )
KOINOS_DEFINE_GET_WASM( system_from_user )
KOINOS_DEFINE_GET_WASM( user_from_system )
KOINOS_DEFINE_GET_WASM( user_from_user )
