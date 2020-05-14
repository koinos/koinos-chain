#include <appbase/application.hpp>

#define KOINOS_BLOCK_PRODUCER_PLUGIN_NAME "block_producer"

namespace koinos { namespace block_producer_plugin {

   using namespace appbase;

   class block_producer_plugin : public appbase::plugin< block_producer_plugin >
   {
      public:
         block_producer_plugin();
         virtual ~block_producer_plugin();

         static const std::string& name() { static std::string name = KOINOS_BLOCK_PRODUCER_PLUGIN_NAME; return name; }

         virtual void set_program_options( options_description&, options_description& ) override {}

         virtual void plugin_initialize( const variables_map& options ) override;
         virtual void plugin_startup() override;
         virtual void plugin_shutdown() override;
   };

   block_producer_plugin::block_producer_plugin() {}
   block_producer_plugin::~block_producer_plugin() {}

   void block_producer_plugin::plugin_initialize( const variables_map& options ) {}

   void block_producer_plugin::plugin_startup() {}
   void block_producer_plugin::plugin_shutdown() {}
} }
