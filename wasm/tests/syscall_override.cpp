
#include <koinos/system/system_calls.hpp>

extern "C" int32_t invoke_thunk( uint32_t sid, char* ret_ptr, uint32_t ret_len, char* arg_ptr, uint32_t arg_len );

int main()
{
   auto args = koinos::system::get_arguments().second;

   koinos::read_buffer rdbuf( (uint8_t*)args.c_str(), args.size() );
   koinos::chain::log_arguments< koinos::system::detail::max_argument_size > log_args;
   log_args.deserialize( rdbuf );

   auto msg = "test: " + std::string( reinterpret_cast< const char* >( log_args.get_message().get_const() ), log_args.get_message().get_length() );
   log_args.mutable_message() = msg.c_str();

   koinos::write_buffer buffer( koinos::system::detail::syscall_buffer.data(), koinos::system::detail::syscall_buffer.size() );
   log_args.serialize( buffer );

   return invoke_thunk(
      std::underlying_type_t< koinos::chain::system_call_id >( koinos::chain::system_call_id::log ),
      reinterpret_cast< char* >( koinos::system::detail::syscall_buffer.data() ),
      std::size( koinos::system::detail::syscall_buffer ),
      reinterpret_cast< char* >( buffer.data() ),
      buffer.get_size()
   );
}
