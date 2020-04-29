#pragma once
#include <koinos/pack/classes.hpp>

struct pack_fixture {};

struct test_object
{
   koinos::protocol::fl_blob< 8 >   id;
   koinos::protocol::multihash_type key;
   std::vector< uint32_t >          vals;
};

KOINOS_REFLECT( test_object, (id)(key)(vals) )
