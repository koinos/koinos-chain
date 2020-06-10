#pragma once

#include <vector>

#include "wasm/hello_wasm.hpp"

struct vm_fixture
{
   std::vector< uint8_t > get_hello_wasm()
   {
      return std::vector< uint8_t >( hello_wasm, hello_wasm + hello_wasm_len );
   }
};

