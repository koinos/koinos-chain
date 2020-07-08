#pragma once
#include <appbase/application.hpp>
#include <koinos/chain/controller.hpp>

#include <filesystem>

#define KOINOS_CHAIN_PLUGIN_NAME "chain"

namespace koinos::plugins::chain {

namespace detail { class chain_plugin_impl; }

class chain_plugin : public appbase::plugin< chain_plugin >
{
public:
   APPBASE_PLUGIN_REQUIRES()

   chain_plugin();
   virtual ~chain_plugin();

   std::filesystem::path state_dir() const;

   static const std::string& name() { static std::string name = KOINOS_CHAIN_PLUGIN_NAME; return name; }

   virtual void set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) override;
   virtual void plugin_initialize( const appbase::variables_map& options ) override;
   virtual void plugin_startup() override;
   virtual void plugin_shutdown() override;

   koinos::chain::controller& controller();
   const koinos::chain::controller& controller() const;

private:
   std::unique_ptr< detail::chain_plugin_impl > my;
};

} // koinos::plugins::chain
