#include <koinos/pack/rt/binary.hpp>
#include <koinos/pack/rt/json.hpp>
#include <koinos/pack/classes.hpp>

#include <koinos/pack/rt/binary.hpp>

struct transfer_args
{
   std::string from;
   std::string to;
   uint64_t    value;
};

KOINOS_REFLECT( transfer_args, (from)(to)(value) );

struct mint_args
{
   std::string to;
   uint64_t    value;
};

KOINOS_REFLECT( mint_args, (to)(value) );
