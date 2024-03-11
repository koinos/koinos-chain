#include <koinos/system/system_calls.hpp>
#include <koinos/chain/system_calls.h>

#include <string>

using namespace std::string_literals;

enum entry {
   echo_call   = 0x01,
   failure_call   = 0x02,
   reversion_call = 0x03,
   write_call     = 0x04
};

int main()
{
   auto [ entry, args ] = koinos::system::get_arguments();

   std::array< uint8_t, 128 > retbuf;
   koinos::read_buffer rdbuf( (uint8_t*)args.c_str(), args.size() );
   koinos::write_buffer buffer( retbuf.data(), retbuf.size() );

   koinos::system::result r;
   int32_t code = 0;

   koinos::chain::error_data< 32 > errdata;

   if ( entry == echo_call )
   {
      errdata.deserialize( rdbuf );
      errdata.serialize( buffer );
      r.mutable_object().set( buffer.data(), buffer.get_size() );
      code = 0;
   }
   else if ( entry == failure_call )
   {
      koinos::system::fail( "failure"s, -1 );
   }
   else if ( entry == reversion_call )
   {
      koinos::system::revert( "reversion"s, 1 );
   }
   else if ( entry == write_call )
   {
      std::string msg = "write";

      koinos::system::object_space space;
      space.set_system( true );
      koinos::system::detail::put_object( space, "\x00"s, msg );
      errdata.mutable_message().set( msg.data(), msg.size() );
      errdata.serialize( buffer );
      r.mutable_object().set( buffer.data(), buffer.get_size() );
      code = 0;
   }

   koinos::system::exit( code, r );

   return 0;
}
