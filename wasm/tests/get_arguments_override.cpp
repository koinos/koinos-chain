#include <koinos/system/system_calls.hpp>

extern "C" int32_t invoke_thunk( uint32_t sid, char* ret_ptr, uint32_t ret_len, char* arg_ptr, uint32_t arg_len, uint32_t* bytes_written );

int main()
{
   std::string args = "override";

   koinos::chain::get_arguments_result< koinos::system::detail::max_argument_size > res;
   res.mutable_value().mutable_arguments().set( reinterpret_cast< const uint8_t* >( args.data() ), args.size() );

   koinos::system::exit( res );

   return 0;
}
