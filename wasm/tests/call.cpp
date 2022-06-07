
#include <koinos/system/system_calls.hpp>

int main()
{
   auto [ entry_point, args ] = koinos::system::get_arguments();

   koinos::read_buffer rdbuf( (uint8_t*)args.c_str(), args.size() );
   koinos::chain::call_arguments< koinos::system::detail::max_hash_size, koinos::system::detail::max_argument_size > call_args;
   call_args.deserialize( rdbuf );

   auto [ code, value ] = koinos::system::call(
      std::string( reinterpret_cast< const char* >( call_args.get_contract_id().get_const() ), call_args.get_contract_id().get_length() ),
      call_args.get_entry_point(),
      std::string( reinterpret_cast< const char* >( call_args.get_args().get_const() ), call_args.get_args().get_length() )
   );

   koinos::chain::result< koinos::system::detail::max_argument_size > r;
   r.set_code( code );
   r.mutable_value().set( reinterpret_cast< uint8_t* >( value.data() ), value.size() );

   koinos::system::exit( 0, r );
}
