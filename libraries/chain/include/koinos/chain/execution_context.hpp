#pragma once

#include <google/protobuf/descriptor.h>

#include <koinos/chain/chronicler.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/resource_meter.hpp>
#include <koinos/chain/session.hpp>
#include <koinos/crypto/elliptic.hpp>
#include <koinos/state_db/state_db.hpp>
#include <koinos/vm_manager/vm_backend.hpp>

#include <koinos/chain/chain.pb.h>
#include <koinos/chain/system_call_ids.pb.h>
#include <koinos/protocol/protocol.pb.h>

#include <deque>
#include <memory>
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
   std::string contract_id;
   uint32_t    sid = 0;
   privilege   call_privilege;
   std::string call_args;
   uint32_t    entry_point = 0;
   std::string call_return;
};

enum class intent : uint64_t
{
   read_only,
   block_application,
   transaction_application
};

struct execution_context_cache
{
   std::map< std::string, uint64_t > compute_bandwidth;
   std::unique_ptr< google::protobuf::DescriptorPool > descriptor_pool;
};

class execution_context
{
   public:
      execution_context() = delete;
      execution_context( std::shared_ptr< vm_manager::vm_backend >, chain::intent i = chain::intent::read_only );

      static constexpr std::size_t stack_limit = 256;

      std::shared_ptr< vm_manager::vm_backend > get_backend() const;

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

      void set_contract_call_args( const std::string& args );
      const std::string& get_contract_call_args() const;

      uint32_t get_contract_entry_point() const;

      std::string get_contract_return() const;
      void set_contract_return( const std::string& ret );

      uint64_t get_compute_bandwidth( const std::string& thunk_name ) const;

      /**
       * For now, authority lives on the context.
       * This should be moved, made generic, or otherwise re-architected.
       */
      void set_key_authority( const crypto::public_key& key );
      void clear_authority();

      void push_frame( stack_frame&& frame );
      stack_frame pop_frame();

      const std::string& get_caller() const;
      privilege get_caller_privilege() const;
      uint32_t get_caller_entry_point() const;

      void set_privilege( privilege );
      privilege get_privilege() const;

      const std::string& get_contract_id() const;

      bool read_only() const;

      chain::resource_meter& resource_meter();
      chain::chronicler& chronicler();

      std::shared_ptr< session > make_session( uint64_t rc );

      void set_intent( chain::intent i );
      chain::intent intent() const;

      chain::receipt& receipt();

      void build_cache();

      const google::protobuf::DescriptorPool& descriptor_pool() const;

   private:
      friend struct frame_restorer;

      std::shared_ptr< vm_manager::vm_backend > _vm_backend;
      std::vector< stack_frame >                _stack;

      abstract_state_node_ptr                   _current_state_node;
      abstract_state_node_ptr                   _parent_state_node;
      std::optional< crypto::public_key >       _key_auth;

      const protocol::block*                    _block = nullptr;
      const protocol::transaction*              _trx = nullptr;

      chain::resource_meter                     _resource_meter;
      chain::chronicler                         _chronicler;

      chain::intent                             _intent;
      chain::receipt                            _receipt;

      execution_context_cache                   _cache;
};

namespace detail {

struct frame_restorer
{
   frame_restorer( execution_context& ctx, stack_frame&& f ) :
      _ctx( ctx )
   {
      _ctx.push_frame( std::move( f ) );
   }

   ~frame_restorer()
   {
      _ctx.pop_frame();
   }

   private:
      execution_context& _ctx;
};

struct privilege_restorer
{
   privilege_restorer( execution_context& ctx, privilege p ) :
      _ctx( ctx )
   {
      _privilege = ctx.get_privilege();
      _ctx.set_privilege( p );
   }

   ~privilege_restorer()
   {
      _ctx.set_privilege( _privilege );
   }

   private:
      execution_context& _ctx;
      privilege          _privilege;
};

} // detail

template< typename Lambda >
void with_stack_frame( execution_context& ctx, stack_frame&& f, Lambda&& l )
{
   detail::frame_restorer r( ctx, std::move( f ) );
   l();
}

template< typename Lambda >
void with_privilege( execution_context& ctx, privilege p, Lambda&& l )
{
   detail::privilege_restorer r( ctx, p );
   l();
}

} // koinos::chain
