#pragma once

#include <appbase/application.hpp>
#include <koinos/plugins/chain/chain_plugin.hpp>
#include <koinos/pack/classes.hpp>
#include <koinos/crypto/elliptic.hpp>

#define KOINOS_BLOCK_PRODUCER_PLUGIN_NAME "block_producer"
#define KOINOS_BLOCK_TIME_MS              10000

using namespace appbase;

namespace koinos::plugins::block_producer {

    class block_producer_plugin : public appbase::plugin< block_producer_plugin >
    {
       public:
          block_producer_plugin();
          virtual ~block_producer_plugin();

          APPBASE_PLUGIN_REQUIRES( (plugins::chain::chain_plugin) );

          static const std::string& name() { static std::string name = KOINOS_BLOCK_PRODUCER_PLUGIN_NAME; return name; }

          std::shared_ptr<protocol::block_header> produce_block();

          virtual void set_program_options( options_description&, options_description& ) override {}

          virtual void plugin_initialize( const variables_map& options ) override;
          virtual void plugin_startup() override;
          virtual void plugin_shutdown() override;

          void demo_create_contract(protocol::active_block_data& active_data);
          void demo_call_contract(protocol::active_block_data& active_data);

          void start_block_production();
          void stop_block_production();

          // Whether or not we should be producing blocks
          // stop_block_production uses this to shut down the production thread
          bool producing_blocks = false;
          crypto::private_key block_signing_private_key;

          std::shared_ptr< std::thread > block_production_thread;
    };
}
