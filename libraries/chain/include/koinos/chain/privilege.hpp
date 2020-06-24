#pragma once
#include <cstdint>
#include <koinos/chain/exceptions.hpp>

namespace koinos::chain {

KOINOS_DECLARE_DERIVED_EXCEPTION( insufficient_privileges, chain_exception );

enum class privilege : uint8_t
{
   kernel_mode,
   user_mode
};

} // koinos::chain
