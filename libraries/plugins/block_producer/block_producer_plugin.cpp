#include <thread>

#include <appbase/application.hpp>
#include <koinos/pack/classes.hpp>
#include <koinos/plugins/chain/chain_plugin.hpp>

#define KOINOS_BLOCK_PRODUCER_PLUGIN_NAME "block_producer"
#define KOINOS_BLOCK_TIME_MS              10000

namespace koinos { namespace block_producer_plugin {

   using namespace appbase;

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
       
         void start_block_production();
         void stop_block_production();
         
         // Whether or not we should be producing blocks
         // stop_block_production uses this to shut down the production thread
         bool producing_blocks = false;
       
         std::shared_ptr< std::thread > block_production_thread;
   };

   static protocol::timestamp_type timestamp_now()
   {
       auto duration = std::chrono::system_clock::now().time_since_epoch();
       auto ticks = std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count();
       
       protocol::timestamp_type t;
       t.timestamp = ticks;
       return t;
   }

   std::shared_ptr< protocol::block_header > block_producer_plugin::produce_block()
   {
       auto block = std::make_shared< protocol::block_header >();
       block->active.timestamp = timestamp_now();
       return block;
   }

   block_producer_plugin::block_producer_plugin() {}
   block_producer_plugin::~block_producer_plugin() {}

   void block_producer_plugin::plugin_initialize( const variables_map& options ) {}

   void block_producer_plugin::plugin_startup()
   {
       start_block_production();
   }

   void block_producer_plugin::plugin_shutdown()
   {
       stop_block_production();
   }

   void block_producer_plugin::start_block_production()
   {
       producing_blocks = true;
       
       block_production_thread = std::make_shared< std::thread >( [&]()
       {
           while ( producing_blocks )
           {
               auto block = produce_block();
               // TODO: Send to chain plugin
               
               // Sleep for the block production time
               boost::this_thread::sleep_for( boost::chrono::milliseconds( KOINOS_BLOCK_TIME_MS ) );
           }
       });
   }

   void block_producer_plugin::stop_block_production()
   {
       producing_blocks = false;
       
       if ( block_production_thread )
           block_production_thread->join();
       
       block_production_thread.reset();
   }
}

}
