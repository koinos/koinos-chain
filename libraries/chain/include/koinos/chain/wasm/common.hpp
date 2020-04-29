#pragma once

#include <koinos/chain/wasm_interface.hpp>

// forward declaraton of alligned_array_wrapper
namespace eosio::vm {
   template< typename T, size_t Align >
   struct aligned_array_wrapper;
}

namespace koinos { namespace chain {

   class apply_context;
   class transaction_context;

   template<typename T>
   struct class_from_wasm {
      /**
       * by default this is just constructing an object
       * @param wasm - the wasm_interface to use
       * @return
       */
      static auto value(apply_context& ctx) {
         return T(ctx);
      }
   };

   template<>
   struct class_from_wasm<transaction_context> {
      /**
       * by default this is just constructing an object
       * @param wasm - the wasm_interface to use
       * @return
       */
      template <typename ApplyCtx>
      static auto &value(ApplyCtx& ctx) {
         return ctx.trx_context;
      }
   };

   template<>
   struct class_from_wasm<apply_context> {
      /**
       * Don't construct a new apply_context, just return a reference to the existing ont
       * @param wasm
       * @return
       */
      static auto &value(apply_context& ctx) {
         return ctx;
      }
   };

   /**
    * class to represent an in-wasm-memory array
    * it is a hint to the transcriber that the next parameter will
    * be a size (data bytes length) and that the pair are validated together
    * This triggers the template specialization of intrinsic_invoker_impl
    * @tparam T
    */
   template<typename T>
   struct array_ptr {
      explicit array_ptr (T * value) : value(value) {}

      template< size_t Align >
      array_ptr( const eosio::vm::aligned_array_wrapper< T, Align >& w ) :
         value( (T*)w.ptr )
      {}

      typename std::add_lvalue_reference<T>::type operator*() const {
         return *value;
      }

      T *operator->() const noexcept {
         return value;
      }

      operator T *() const {
         return value;
      }

      T *value;
   };

   struct null_terminated_ptr {
      explicit null_terminated_ptr(char* value) : value(value) {}

      typename std::add_lvalue_reference<char>::type operator*() const {
         return *value;
      }

      char *operator->() const noexcept {
         return value;
      }

      operator char *() const {
         return value;
      }

      char *value;
   };

} } // koinos::chain
