#include <koinos/system/system_calls.hpp>

int main()
{
   auto [ entry, args ] = koinos::system::get_arguments();

   koinos::system::result r;
   r.mutable_object().set( reinterpret_cast< const uint8_t* >( args.data() ), args.size() );
   koinos::system::exit( 0, r );

   return 0;
}
