#pragma once

#include <koinos/exception.hpp>

#define KOINOS_PACK_MAX_ARRAY_ALLOC_SIZE (1024*1024*10)
#define KOINOS_PACK_MAX_RECURSION_DEPTH  (20)

namespace koinos::pack {

/*
 * Generic serialization error. Any uses should consider being replaced
 * with a more specific/descriptire exception.
 */
DECLARE_KOINOS_EXCEPTION( parse_error );

/*
 * Parsing input recursed too deep.
 */
DECLARE_KOINOS_EXCEPTION( depth_violation );

/*
 * Parsing input would require allocation too much memory
 */
DECLARE_KOINOS_EXCEPTION( allocation_violation );

/*
 * Unexpected end of stream while unpacking from binary.
 */
DECLARE_KOINOS_EXCEPTION( stream_error );

/*
 * JSON parsing is out of bounds for the destination integer type.
 */
DECLARE_KOINOS_EXCEPTION( json_int_out_of_bounds );

/*
 * There was a problem serializing to the JSON object. This is probably
 * caused by programmer error.
 */
DECLARE_KOINOS_EXCEPTION( json_serialization_error );

/*
 * Incoming JSON type does not match expected type.
 */
DECLARE_KOINOS_EXCEPTION( json_type_mismatch );

/*
 * There was a problem decoding an encoded byte string
 */
DECLARE_KOINOS_EXCEPTION( json_decode_error );


DECLARE_KOINOS_EXCEPTION( bad_cast_exception );

} // koinos::pack
