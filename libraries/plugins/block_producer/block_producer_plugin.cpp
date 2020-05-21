#include <thread>
#include <boost/interprocess/streams/vectorstream.hpp>

#include <koinos/plugins/block_producer/block_producer_plugin.hpp>
#include <koinos/pack/rt/binary.hpp>
#include <koinos/crypto/multihash.hpp>

namespace koinos::plugins::block_producer {

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
       // TODO: block->active.height = get_height();
       
       // Sign the block
       auto digest = crypto::hash<protocol::active_block_data>(CRYPTO_SHA2_256_ID, block->active);
       auto signature = block_signing_private_key.sign_compact(digest);
       block->passive.block_signature = signature;

       // TODO: Get block height
       // TODO: get previous

       // Serialize the active data
       boost::interprocess::basic_vectorstream< std::vector<char> > stream;
       protocol::to_binary(stream, block->active);
       crypto::vl_blob active_bytes;
       active_bytes.data = stream.vector();

       return block;
   }

   block_producer_plugin::block_producer_plugin() {}
   block_producer_plugin::~block_producer_plugin() {}

   void block_producer_plugin::plugin_initialize( const variables_map& options )
   {
       std::string seed = "test seed";
       block_signing_private_key.regenerate(crypto::hash(CRYPTO_SHA2_256_ID, seed.c_str(), seed.size()));
   }

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
