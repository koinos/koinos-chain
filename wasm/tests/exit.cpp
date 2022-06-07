
#include <koinos/system/system_calls.hpp>

int main()
{
   auto [ entry_point, args ] = koinos::system::get_arguments();

   koinos::read_buffer rdbuf( (uint8_t*)args.c_str(), args.size() );
   koinos::chain::result< koinos::system::detail::max_argument_size > result_args;
   result_args.deserialize( rdbuf );

   koinos::system::exit( result_args );
}
