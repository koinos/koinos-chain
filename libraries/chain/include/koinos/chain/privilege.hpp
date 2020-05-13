#pragma once
#include <cstdint>
#include <koinos/exception.hpp>

namespace koinos::chain {

DECLARE_KOINOS_EXCEPTION( insufficient_privileges );

enum class privilege : uint8_t
{
   kernel_mode,
   user_mode
};

} // koinos::kernel
