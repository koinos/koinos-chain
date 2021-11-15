#pragma once

#include <koinos/chain/event_recorder.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/resource_meter.hpp>
#include <koinos/chain/session.hpp>
#include <koinos/crypto/elliptic.hpp>
#include <koinos/state_db/state_db.hpp>
#include <koinos/vm_manager/vm_backend.hpp>

#include <koinos/chain/chain.pb.h>
#include <koinos/protocol/protocol.pb.h>

#include <deque>
#include <optional>
#include <string>
#include <variant>

namespace koinos::chain {

using boost::container::flat_set;
using koinos::state_db::state_node_ptr;
using koinos::state_db::anonymous_state_node_ptr;
using koinos::state_db::abstract_state_node;

using abstract_state_node_ptr = std::shared_ptr< abstract_state_node >;
using receipt                 = std::variant< std::monostate, protocol::block_receipt, protocol::transaction_receipt >;

struct stack_frame
{
   std::vector< std::byte > call;
   privilege                call_privilege;
   std::vector< std::byte > call_args;
   uint32_t                 entry_point = 0;
   std::vector< std::byte > call_return;
};

enum class intent : uint64_t
{
   read_only,
   block_application,
   transaction_application
};

class execution_context
{
   public:
      execution_context() = delete;
      execution_context( std::shared_ptr< vm_manager::vm_backend >, chain::intent i = chain::intent::read_only );

      static constexpr std::size_t stack_limit = 256;

      std::shared_ptr< vm_manager::vm_backend > get_backend() const;

      /// Console methods:
      void console_append( const std::string& val );
      std::string get_pending_console_output();

      void set_state_node( abstract_state_node_ptr, abstract_state_node_ptr = abstract_state_node_ptr() );
      abstract_state_node_ptr get_state_node() const;
      abstract_state_node_ptr get_parent_node() const;
      void clear_state_node();

      void set_block( const protocol::block& );
      const protocol::block* get_block() const;
      void clear_block();

      void set_transaction( const protocol::transaction& );
      const protocol::transaction& get_transaction() const;
      void clear_transaction();

      void set_contract_call_args( const std::vector< std::byte >& args );
      const std::vector< std::byte >& get_contract_call_args() const;

      uint32_t get_contract_entry_point() const;

      std::vector< std::byte > get_contract_return() const;
      void set_contract_return( const std::vector< std::byte >& ret );

      /**
       * For now, authority lives on the context.
       * This should be moved, made generic, or otherwise re-architected.
       */
      void set_key_authority( const crypto::public_key& key );
      void clear_authority();

      void push_frame( stack_frame&& frame );
      stack_frame pop_frame();

      const std::vector< std::byte >& get_caller()const;
      privilege get_caller_privilege()const;

      void set_privilege( privilege );
      privilege get_privilege()const;

      void set_user_code( bool );
      bool user_code()const;

      bool read_only()const;

      chain::resource_meter& resource_meter();
      chain::event_recorder& event_recorder();

      std::shared_ptr< session > make_session( uint64_t rc );

      void set_intent( chain::intent i );
      chain::intent intent() const;

      chain::receipt& receipt();

   private:
      friend struct frame_restorer;

      std::shared_ptr< vm_manager::vm_backend > _vm_backend;
      std::vector< stack_frame >                _stack;

      abstract_state_node_ptr                   _current_state_node;
      abstract_state_node_ptr                   _parent_state_node;
      std::string                               _pending_console_output;
      std::optional< crypto::public_key >       _key_auth;

      bool                                      _user_code = false;

      const protocol::block*                    _block = nullptr;
      const protocol::transaction*              _trx = nullptr;

      chain::resource_meter                     _resource_meter;
      chain::event_recorder                     _event_recorder;

      chain::intent                             _intent;
      chain::receipt                            _receipt;
};

struct frame_restorer
{
   frame_restorer( execution_context& ctx, stack_frame&& f ) :
      _ctx( ctx )
   {
      _user_code = _ctx.user_code();
      if ( f.call_privilege == privilege::user_mode )
      {
         _ctx.set_user_code( true );
      }
      _ctx.push_frame( std::move( f ) );
   }

   ~frame_restorer()
   {
      _ctx.pop_frame();
      _ctx.set_user_code( _user_code );
   }

   private:
      execution_context& _ctx;
      bool           _user_code;
};

template< typename Lambda >
void with_stack_frame( execution_context& ctx, stack_frame&& f, Lambda&& l )
{
   frame_restorer r( ctx, std::move( f ) );
   l();
}

} // koinos::chain
