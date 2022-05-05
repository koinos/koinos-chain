#include <koinos/system/system_calls.hpp>

int main()
{
   auto [ entry, args ] = koinos::system::get_arguments();

   koinos::system::result r;
   r.set_code( 0 );
   r.mutable_value().set( reinterpret_cast< const uint8_t* >( args.data() ), args.size() );
   koinos::system::exit( r );

   return 0;
}
