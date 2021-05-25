
#include <koinos/tests/capsule/dummy_hash.hpp>

namespace koinos::capsule {

std::string dummy_hash_to_string( dummy_hash_ptr h )
{
   if( !h )
      return "nil";
   return std::visit(
      []( auto& v ) -> std::string
      {
         if( !v )
            return "nil";
         return v->to_string();
      }, *h );
}

dummy_hash_ptr create_dummy_hash(int64_t value)
{
   return std::make_shared< dummy_hash >( std::make_shared< simple_dummy_hash >( value ) );
}

dummy_hash_ptr reduce_dummy_hash(dummy_hash_ptr a, dummy_hash_ptr b)
{
   return std::make_shared< dummy_hash >( std::make_shared< complex_dummy_hash >( a, b ) );
}

std::ostream& operator<<( std::ostream& o, dummy_hash_ptr h )
{
   o << dummy_hash_to_string(h);
   return o;
}

}
