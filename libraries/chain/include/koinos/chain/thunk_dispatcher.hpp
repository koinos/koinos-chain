#pragma once

#include <cstdint>

#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/register_thunks.hpp>
#include <koinos/chain/thunks.hpp>
#include <koinos/exception.hpp>

#include <koinos/pack/classes.hpp>
#include <koinos/pack/rt/binary.hpp>

#include <boost/container/flat_map.hpp>

#include <any>
#include <functional>
#include <type_traits>

namespace koinos::chain {

DECLARE_KOINOS_EXCEPTION( unknown_thunk );

namespace detail
{
   /*
    * std::apply takes a function and a tuple and calls the function with the contents of the tuple
    * as the arguments. The only "trick" here is converting a reflected object in to an equivalent
    * tuple. That is handled as part of the reflection macro to generate the needed code. We cat a
    * tuple containing just the apply context as the first argument and then call in to the thunk.
    *
    * Two versions exist of the function, one that serializes the return value and one that does not.
    */
   template< typename ArgStruct, typename ThunkReturn, typename... ThunkArgs >
   typename std::enable_if< std::is_same< ThunkReturn, void >::value, int >::type
   call_thunk_impl( const std::function< ThunkReturn(apply_context&, ThunkArgs...) >& thunk, apply_context& ctx, char* ret_ptr, uint32_t ret_len, ArgStruct& arg )
   {
      auto thunk_args = std::tuple_cat( std::tuple< apply_context& >( ctx ), pack::reflector< ArgStruct >::make_tuple( arg ) );
      std::apply( thunk, thunk_args );
      return 0;
   }

   template< typename ArgStruct, typename ThunkReturn, typename... ThunkArgs >
   typename std::enable_if< !std::is_same< ThunkReturn, void >::value, int >::type
   call_thunk_impl( const std::function< ThunkReturn(apply_context&, ThunkArgs...) >& thunk, apply_context& ctx, char* ret_ptr, uint32_t ret_len, ArgStruct& arg )
   {
      auto thunk_args = std::tuple_cat( std::tuple< apply_context& >( ctx ), pack::reflector< ArgStruct >::make_tuple( arg ) );
      pack::to_c_str< ThunkReturn >( ret_ptr, ret_len, std::apply( thunk, thunk_args ) );
      return 0;
   }

} // detail

/**
 * A registry for thunks.
 *
 * - A thunk is one-directional:  It's a call from WASM to native C++ code.
 * - A thunk is immutable:  A thunk always points to a particular native function.
 *
 * In particular, a semantically inequivalent upgrade to a thunk should never occur.
 * When there's a bug in a thunk implementation, the existing buggy implementation should
 * be kept under the same thunk ID.
 *
 * System governance should instead replace the system code that calls a particular
 * thunk ID with system code that calls a different thunk ID.
 *
 * When upgrading a system call from one thunk to another, the new thunk **MUST**
 * have identical function signature to the existing thunk.
 */
class thunk_dispatcher
{
   public:
      void call_thunk( thunk_id id, apply_context& ctx, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len )const;

      template< typename ThunkReturn, typename... ThunkArgs >
      auto call_thunk( uint32_t id, apply_context& ctx, ThunkArgs&... args ) const
      {
         auto it = _pass_through_map.find( id );
         KOINOS_ASSERT( it != _pass_through_map.end(), unknown_thunk, "Thunk ${id} not found", ("id", id) );
         return std::any_cast< std::function<ThunkReturn(apply_context&, ThunkArgs...)> >(it->second)( ctx, args... );
      }

      template< typename ArgStruct, typename ThunkReturn, typename... ThunkArgs >
      void register_thunk( thunk_id id, ThunkReturn (*thunk_ptr)(apply_context&, ThunkArgs...) )
      {
         std::function<ThunkReturn(apply_context&, ThunkArgs...)> thunk = thunk_ptr;
         _dispatch_map.emplace( id, [thunk]( apply_context& ctx, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len )
         {
            ArgStruct args;
            koinos::pack::from_c_str( arg_ptr, arg_len, args );
            detail::call_thunk_impl( thunk, ctx, ret_ptr, ret_len, args );
         });
         _pass_through_map.emplace( id, thunk );
      }

      static const thunk_dispatcher& instance();

   private:
      thunk_dispatcher();

      typedef std::function< void(apply_context&, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len) > generic_thunk_handler;

      boost::container::flat_map< thunk_id, generic_thunk_handler >  _dispatch_map;
      boost::container::flat_map< thunk_id, std::any >               _pass_through_map;
};

} // koinos::chain
