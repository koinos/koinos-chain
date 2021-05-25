#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>

/**
 * Define a dummy hash data type for capsule tests.
 *
 * The dummy hash function only hashes integers.
 * The hash of an integer i is defined to be "h" + str(i).
 * The hash of a pair of hashes ha, hb is defined to be "(ha, hb)"
 */

namespace koinos::capsule {

class simple_dummy_hash;
class complex_dummy_hash;

typedef std::shared_ptr< simple_dummy_hash > simple_dummy_hash_ptr;
typedef std::shared_ptr< complex_dummy_hash > complex_dummy_hash_ptr;
typedef std::variant<
   simple_dummy_hash_ptr,
   complex_dummy_hash_ptr
   > dummy_hash;
typedef std::shared_ptr< dummy_hash > dummy_hash_ptr;

/**
 * Convert a dummy hash to string.
 */
std::string dummy_hash_to_string( dummy_hash_ptr h );

/**
 * A simple dummy hash directly hashes a value.
 */
class simple_dummy_hash
{
   public:
      simple_dummy_hash( int64_t v ) : value(v) {}
      std::string to_string() { return std::string("h") + std::to_string(value); }

   private:
      int64_t value;
};

/**
 * A complex dummy hash hashes two hashes.
 */
class complex_dummy_hash
{
   public:
      complex_dummy_hash( dummy_hash_ptr a, dummy_hash_ptr b ) : first(a), second(b) {}
      std::string to_string() { return std::string("(")
                                     + dummy_hash_to_string(first) + std::string(", ")
                                     + dummy_hash_to_string(second) + std::string(")"); }
   private:
      dummy_hash_ptr first, second;
};

/**
 * Create a simple dummy hash conveniently.
 */
dummy_hash_ptr create_dummy_hash(int64_t value);

/**
 * Create a complex dummy hash conveniently.
 */
dummy_hash_ptr reduce_dummy_hash(dummy_hash_ptr a, dummy_hash_ptr b);

std::ostream& operator<<( std::ostream& o, dummy_hash_ptr h );

}
