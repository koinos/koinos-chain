#pragma once
#include <koinos/vmmanager/eos/common.hpp>
#include <koinos/vmmanager/eos/exceptions.hpp>

namespace eosio::vm {

   template<typename T>
   struct wasm_type_converter<T*> : linear_memory_access {
      auto from_wasm(void* val) {
         validate_ptr<T>(val, 1);
         return eosio::vm::aligned_ptr_wrapper<T, alignof(T)>{val};
      }
   };

   template<>
   struct wasm_type_converter<char*> : linear_memory_access {
      void* to_wasm(char* val) {
         validate_ptr<char>(val, 1);
         return val;
      }
   };

   template<typename T>
   struct wasm_type_converter<T&> : linear_memory_access {
      auto from_wasm(uint32_t val) {
         KOINOS_ASSERT( val != 0, koinos::vmmanager::eos::wasm_type_conversion_exception, "references cannot be created for null pointers" );
         void* ptr = get_ptr(val);
         validate_ptr<T>(ptr, 1);
         return eosio::vm::aligned_ref_wrapper<T, alignof(T)>{ptr};
      }
   };

   template<typename T>
   struct wasm_type_converter<koinos::vmmanager::eos::array_ptr<T>> : linear_memory_access {
      auto from_wasm(void* ptr, uint32_t size) {
         validate_ptr<T>(ptr, size);
         return aligned_array_wrapper<T, alignof(T)>(ptr, size);
      }
   };

   template<>
   struct wasm_type_converter<koinos::vmmanager::eos::array_ptr<char>> : linear_memory_access {
      auto from_wasm(void* ptr, uint32_t size) {
         validate_ptr<char>(ptr, size);
         return koinos::vmmanager::eos::array_ptr<char>((char*)ptr);
      }
      // memcpy/memmove
      auto from_wasm(void* ptr, koinos::vmmanager::eos::array_ptr<const char> /*src*/, uint32_t size) {
         validate_ptr<char>(ptr, size);
         return koinos::vmmanager::eos::array_ptr<char>((char*)ptr);
      }
      // memset
      auto from_wasm(void* ptr, int /*val*/, uint32_t size) {
         validate_ptr<char>(ptr, size);
         return koinos::vmmanager::eos::array_ptr<char>((char*)ptr);
      }
   };

   template<>
   struct wasm_type_converter<koinos::vmmanager::eos::array_ptr<const char>> : linear_memory_access {
      auto from_wasm(void* ptr, uint32_t size) {
         validate_ptr<char>(ptr, size);
         return koinos::vmmanager::eos::array_ptr<const char>((char*)ptr);
      }
      // memcmp
      auto from_wasm(void* ptr, koinos::vmmanager::eos::array_ptr<const char> /*src*/, uint32_t size) {
         validate_ptr<char>(ptr, size);
         return koinos::vmmanager::eos::array_ptr<const char>((char*)ptr);
      }
   };

   template <>
   struct construct_derived<koinos::vmmanager::eos::apply_context, koinos::vmmanager::eos::apply_context> {
      static auto &value(koinos::vmmanager::eos::apply_context& ctx) { return ctx; }
   };

   template<>
   struct wasm_type_converter<koinos::vmmanager::eos::null_terminated_ptr> : linear_memory_access {
      auto from_wasm(void* ptr) {
         validate_c_str(ptr);
         return koinos::vmmanager::eos::null_terminated_ptr{ static_cast<char*>(ptr) };
      }
   };
}
