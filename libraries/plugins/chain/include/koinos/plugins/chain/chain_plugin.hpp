#pragma once
#include <appbase/application.hpp>
#include <chainbase/chainbase.hpp>

#define KOINOS_CHAIN_PLUGIN_NAME "chain"

namespace koinos::plugins::chain {

namespace detail { class chain_plugin_impl; }

using namespace appbase;

namespace bfs = boost::filesystem;

class chain_plugin : public plugin< chain_plugin >
{
public:
   APPBASE_PLUGIN_REQUIRES()

   chain_plugin();
   virtual ~chain_plugin();

   bfs::path state_dir() const;

   static const std::string& name() { static std::string name = KOINOS_CHAIN_PLUGIN_NAME; return name; }

   virtual void set_program_options( options_description& cli, options_description& cfg ) override;
   virtual void plugin_initialize( const variables_map& options ) override;
   virtual void plugin_startup() override;
   virtual void plugin_shutdown() override;

   chainbase::database& db();
   const chainbase::database& db() const;

private:
   std::unique_ptr< detail::chain_plugin_impl > my;
};

} // koinos::plugins::chain
