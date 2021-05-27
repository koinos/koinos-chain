#pragma once

#include <koinos/chain/exceptions.hpp>
#include <koinos/statedb/statedb.hpp>
#include <koinos/pack/classes.hpp>
#include <koinos/crypto/elliptic.hpp>

#include <deque>
#include <optional>
#include <string>

#define APPLY_CONTEXT_STACK_LIMIT 256

namespace koinos::chain {

using boost::container::flat_set;
using koinos::statedb::state_node_ptr;

struct stack_frame
{
   account_type  call;
   privilege     call_privilege;
   variable_blob call_args;
   uint32_t      entry_point = 0;
   variable_blob call_return;
};

class apply_context
{
   public:
      apply_context() = default;

      /// Console methods:
      void console_append( const std::string& val );
      std::string get_pending_console_output();

      void set_state_node( state_node_ptr );
      state_node_ptr get_state_node() const;
      void clear_state_node();

      void set_block( const protocol::block& );
      const protocol::block* get_block() const;
      void clear_block();

      void set_transaction( const protocol::transaction& );
      const protocol::transaction& get_transaction() const;
      void clear_transaction();

      void set_contract_call_args( const variable_blob& args );
      const variable_blob& get_contract_call_args() const;

      uint32_t get_contract_entry_point() const;

      variable_blob get_contract_return() const;
      void set_contract_return( const variable_blob& ret );

      /**
       * For now, authority lives on the context.
       * This should be moved, made generic, or otherwise re-architected.
       */
      void set_key_authority( const crypto::public_key& key );
      void clear_authority();

      void push_frame( stack_frame&& frame );
      stack_frame pop_frame();

      const account_type& get_caller()const;
      privilege get_caller_privilege()const;

      void set_privilege( privilege );
      privilege get_privilege()const;

      void set_in_user_code( bool );
      bool is_in_user_code()const;

      void set_read_only( bool );
      bool is_read_only()const;

      std::vector< stack_frame >             _stack;

   private:
      friend struct frame_restorer;

      state_node_ptr                         _current_state_node;
      std::string                            _pending_console_output;
      std::optional< crypto::public_key >    _key_auth;

      bool                                   _is_in_user_code = false;
      bool                                   _read_only = false;


      const protocol::block*                 _block = nullptr;
      const protocol::transaction*           _trx = nullptr;
};

struct frame_restorer
{
   frame_restorer( apply_context& ctx, stack_frame&& f ) :
      _ctx( ctx )
   {
      _user_code = _ctx.is_in_user_code();
      if ( f.call_privilege == privilege::user_mode )
      {
         _ctx.set_in_user_code( true );
      }
      _ctx.push_frame( std::move( f ) );
   }

   ~frame_restorer()
   {
      _ctx.pop_frame();
      _ctx.set_in_user_code( _user_code );
   }

   private:
      apply_context& _ctx;
      bool           _user_code;
};

template< typename Lambda >
void with_stack_frame( apply_context& ctx, stack_frame&& f, Lambda&& l )
{
   frame_restorer r( ctx, std::move( f ) );
   l();
}

} // koinos::chain
