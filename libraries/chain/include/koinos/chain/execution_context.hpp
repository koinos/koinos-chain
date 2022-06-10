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
#include <tuple>
#include <utility>
#include <variant>

namespace koinos::chain {

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
};

struct execution_result
{
   int32_t code = 0;
   result  res;
};

enum class intent : uint64_t
{
   read_only,
   block_application,
   transaction_application
};

struct execution_context_cache
{
   using system_call_cache_bundle = std::tuple< std::string, std::string, uint32_t, chain::contract_metadata_object >;

   std::optional< std::map< std::string, uint64_t > > compute_bandwidth;
   std::optional< google::protobuf::DescriptorPool > descriptor_pool;
   std::map< uint32_t, std::variant< system_call_cache_bundle, uint32_t > > system_call_table;
   std::optional< crypto::multicodec > block_hash_code;
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
      const protocol::transaction* get_transaction() const;
      void clear_transaction();

      void set_contract_call_args( const std::string& args );
      const std::string& get_contract_call_args() const;

      uint32_t get_contract_entry_point() const;

      uint64_t get_compute_bandwidth( const std::string& thunk_name );

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

      void reset_cache();

      const google::protobuf::DescriptorPool& descriptor_pool();

      const execution_result& system_call( uint32_t id, const std::string& args );
      uint32_t thunk_translation( uint32_t id );
      bool system_call_exists( uint32_t id );
      const crypto::multicodec& block_hash_code();

      void set_result( const execution_result& r );
      void set_result( execution_result&& r );

      const execution_result& get_result();

   private:
      void build_compute_registry_cache();
      void build_descriptor_pool();
      void cache_system_call( uint32_t );
      void build_block_hash_code_cache();

      std::shared_ptr< vm_manager::vm_backend > _vm_backend;
      std::vector< stack_frame >                _stack;

      abstract_state_node_ptr                   _current_state_node;
      abstract_state_node_ptr                   _parent_state_node;

      const protocol::block*                    _block = nullptr;
      const protocol::transaction*              _trx = nullptr;

      chain::resource_meter                     _resource_meter;
      chain::chronicler                         _chronicler;

      chain::intent                             _intent;
      chain::receipt                            _receipt;

      execution_context_cache                   _cache;
      execution_result                          _result;
};

namespace detail {

struct frame_guard
{
   frame_guard( execution_context& ctx, stack_frame&& f ) :
      _ctx( ctx )
   {
      _ctx.push_frame( std::move( f ) );
   }

   ~frame_guard()
   {
      _ctx.pop_frame();
   }

   private:
      execution_context& _ctx;
};

struct privilege_guard
{
   privilege_guard( execution_context& ctx, privilege p ) :
      _ctx( ctx )
   {
      _privilege = ctx.get_privilege();
      _ctx.set_privilege( p );
   }

   ~privilege_guard()
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
   detail::frame_guard r( ctx, std::move( f ) );
   l();
}

template< typename Lambda >
void with_privilege( execution_context& ctx, privilege p, Lambda&& l )
{
   detail::privilege_guard r( ctx, p );
   l();
}

} // koinos::chain
