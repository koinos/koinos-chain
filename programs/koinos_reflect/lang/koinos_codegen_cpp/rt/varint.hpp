#pragma once

namespace koinos::pack {

/* signed_int and unsigned_int are dumb wrappers around 64 bit integer types
 * for use in varint serialization. They are not intended to be used for
 * anything else.
 */

struct signed_int
{
   signed_int( int64_t v ) :
      value( v )
   {}

   signed_int() = default;

   int64_t value = 0;
};

struct unsigned_int
{
   unsigned_int( uint64_t v ) :
      value( v )
   {}

   unsigned_int() = default;

   uint64_t value = 0;
};

} // koinos::pack
