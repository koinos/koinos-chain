#pragma once

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
