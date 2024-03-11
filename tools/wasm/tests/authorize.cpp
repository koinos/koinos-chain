#include <koinos/system/system_calls.hpp>

#include <koinos/chain/authority.h>

using namespace std::string_literals;

const std::string pub_key = "\x03\x88\xed\xcd\x72\x73\xe3\x4d\x89\xf1\xf2\x3d\x1f\xdb\xdb\xd9\x48\xc6\xcb\xcf\xfb\x6c\x13\xbc\xd4\x50\x39\xee\xc3\x37\x7e\x42\xe5"s;

int main()
{
   koinos::chain::authorize_arguments<
      koinos::system::detail::max_hash_size,
      koinos::system::detail::max_argument_size,
      koinos::system::detail::max_argument_size > auth_args;

   auto [ entry_point, arg_bytes ] = koinos::system::get_arguments();
   koinos::read_buffer rdbuf( reinterpret_cast< const uint8_t* >( arg_bytes.data() ), arg_bytes.size() );

   auth_args.deserialize( rdbuf );

   koinos::chain::authorize_result res;

   // If there is any auxilarly data, return true (for testing only)
   if ( auth_args.mutable_call().mutable_data().get_length() == 1 && auth_args.mutable_call().mutable_data().get( 0 ) )
   {
      res.set_value( 1 );
      koinos::system::exit( res );
   }

   auto id_field_bytes = koinos::system::get_transaction_field( "id" ).get_bytes_value();
   auto id = std::string( reinterpret_cast< const char* >( id_field_bytes.get_const() ), id_field_bytes.get_length() );

   koinos::chain::list_type< 10, 32, 128 > signatures;
   koinos::system::get_transaction_field( "signatures" ).get_message_value().UnpackTo( signatures );

   for ( uint32_t i = 0; i < signatures.values().get_length(); i++ )
   {
      auto sig = std::string( reinterpret_cast< const char* >( signatures.values( i ).get_bytes_value().get_const() ), signatures.values( i ).get_bytes_value().get_length() );
      if ( koinos::system::verify_signature( koinos::chain::dsa::ecdsa_secp256k1, pub_key, id, sig ) )
      {
         res.set_value( 1 );
         break;
      }
   }

   koinos::system::exit( res );

   return 0;
}
