#pragma once
#include <appbase/application.hpp>

// TODO: These three includes go away when submit() is refactored out
#include <koinos/pack/classes.hpp>
#include <future>
#include <memory>

#define KOINOS_CHAIN_PLUGIN_NAME "chain"

namespace koinos::plugins::chain {

namespace detail { class chain_plugin_impl; }

namespace bfs = boost::filesystem;

class chain_plugin : public appbase::plugin< chain_plugin >
{
public:
   APPBASE_PLUGIN_REQUIRES()

   chain_plugin();
   virtual ~chain_plugin();

   bfs::path state_dir() const;

   std::future< std::shared_ptr< koinos::types::rpc::submission_result > > submit( const koinos::types::rpc::submission_item& item );

   static const std::string& name() { static std::string name = KOINOS_CHAIN_PLUGIN_NAME; return name; }

   virtual void set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) override;
   virtual void plugin_initialize( const appbase::variables_map& options ) override;
   virtual void plugin_startup() override;
   virtual void plugin_shutdown() override;

private:
   std::unique_ptr< detail::chain_plugin_impl > my;
};

} // koinos::plugins::chain
