#pragma once
#include <koinos/pack/classes.hpp>

struct pack_fixture {};

struct extensions {};

struct test_object
{
   koinos::types::fixed_blob< 8 > id;
   koinos::types::multihash_type  key;
   std::vector< uint32_t >        vals;
   extensions                     ext;
};

KOINOS_REFLECT( test_object, (id)(key)(vals)(ext) )

KOINOS_REFLECT( extensions, )
