
#include <koinos/system/system_calls.hpp>

int main()
{
   auto [ entry_point, args ] = koinos::system::get_arguments();

   koinos::read_buffer rdbuf( (uint8_t*)args.c_str(), args.size() );
   koinos::system::exit_arguments exit_args;
   exit_args.deserialize( rdbuf );

   koinos::system::exit( exit_args.code(), exit_args.res() );
}
