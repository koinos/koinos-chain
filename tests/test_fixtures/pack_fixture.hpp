#pragma once
#include <koinos/pack/classes.hpp>

struct pack_fixture {};

struct extensions {};

struct test_object
{
   koinos::protocol::fl_blob< 8 >   id;
   koinos::protocol::multihash_type key;
   std::vector< uint32_t >          vals;
   extensions                       ext;
};

KOINOS_REFLECT( test_object, (id)(key)(vals)(ext) )

KOINOS_REFLECT( extensions, )
