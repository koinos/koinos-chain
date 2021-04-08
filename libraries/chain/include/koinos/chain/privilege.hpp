#pragma once
#include <cstdint>

namespace koinos::chain {

enum class privilege : uint8_t
{
   kernel_mode,
   user_mode
};

} // koinos::chain
