#include <thread>
#include <chrono>

#include <boost/filesystem.hpp>
#include <boost/interprocess/streams/vectorstream.hpp>
#include <boost/thread.hpp>

#include <koinos/pack/classes.hpp>
#include <koinos/plugins/block_producer/block_producer_plugin.hpp>
#include <koinos/plugins/block_producer/util/block_util.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/log.hpp>
#include <koinos/util.hpp>

namespace koinos::plugins::block_producer {

using vectorstream = boost::interprocess::basic_vectorstream< std::vector< char > >;
namespace bfs = boost::filesystem;

static timestamp_type timestamp_now()
{
   auto duration = std::chrono::system_clock::now().time_since_epoch();
   auto ticks = std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count();

   timestamp_type t;
   t = ticks;
   return t;
}

std::shared_ptr< protocol::block > block_producer_plugin::produce_block()
{
   // Make block header
   auto block = std::make_shared< protocol::block >();

   // Make active data, fetch timestamp
   protocol::active_block_data active_data;
   active_data.timestamp = timestamp_now();
   active_data.header_hashes.digests.resize( size_t(protocol::header_hash_index::NUM_HEADER_HASHES) );

   // Get previous block data
   block_topology topology;

   chain::chain_plugin& ch = appbase::app().get_plugin< chain::chain_plugin >();
//   auto r = ch.submit( types::rpc::query_submission( chain::rpc::get_head_info_params() ) );
//
//   try
//   {
//      auto res = std::get< types::rpc::query_submission_result >( *r.get() );
//      res.unbox();
//      std::visit( koinos::overloaded {
//         [&]( const types::rpc::get_head_info_result& head_info ) {
//            active_data.height = head_info.height + 1;
//            active_data.header_hashes.digests[(uint32_t)types::protocol::header_hash_index::previous_block_hash_index] = head_info.id.digest;
//            topology.previous = head_info.id;
//            topology.height = active_data.height;
//         },
//         []( const auto& ){}
//      }, res.get_const_native() );
//   }
//   catch ( const std::exception &e )
//   {
//      LOG(error) << e.what();
//      return block;
//   }

   // TODO: Add transactions from the mempool

   // Add passive data
   block->passive_data = protocol::passive_block_data();

   // Serialize active data, store it in block header
   block->active_data = std::move( active_data );

   util::set_block_merkle_roots( *block, CRYPTO_SHA2_256_ID );
   util::sign_block( *block, block_signing_private_key );

   // Store hash of header as ID
   topology.id = crypto::hash( CRYPTO_SHA2_256_ID, block->active_data );

   // Submit the block
//   r = ch.submit( types::rpc::block_submission{
//      .topology = topology,
//      .block = *block,
//      .verify_passive_data = true,
//      .verify_block_signature = true,
//      .verify_transaction_signatures = true } );
//   try
//   {
//      r.get(); // TODO: Probably should do something better here, rather than discarding the result...
//   }
//   catch ( const std::exception &e )
//   {
//      LOG(error) << e.what();
//      block.reset();
//   }

   LOG(info) << "produced block: " << topology;

   return block;
}

block_producer_plugin::block_producer_plugin() {}
block_producer_plugin::~block_producer_plugin() {}

void block_producer_plugin::set_program_options( options_description& cli, options_description& cfg )
{
   cfg.add_options()
         ("target-wasm", bpo::value<bfs::path>(),
            "the location of a demo wasm file (absolute path or relative to application data dir)")
         ;
}

void block_producer_plugin::plugin_initialize( const appbase::variables_map& options )
{
   std::string seed = "test seed";

   block_signing_private_key = crypto::private_key::regenerate( crypto::hash_str( CRYPTO_SHA2_256_ID, seed.c_str(), seed.size() ) );

   if( options.count("target-wasm") )
   {
      auto wasm_target = options.at("target-wasm").as<bfs::path>();
      if( !bfs::is_directory( wasm_target ) )
      {
         if( wasm_target.is_relative() )
            wasm = appbase::app().data_dir() / wasm_target;
         else
            wasm = wasm_target;
      }
   }
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

   block_production_thread = std::make_shared< std::thread >( [&]() {
      std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );

      while ( producing_blocks )
      {
         auto block = produce_block();

         // Sleep for the block production time
         std::this_thread::sleep_for( std::chrono::milliseconds( KOINOS_BLOCK_TIME_MS ) );
      }
   } );
}

void block_producer_plugin::stop_block_production()
{
   producing_blocks = false;

   if ( block_production_thread )
      block_production_thread->join();

   block_production_thread.reset();
}

} // koinos::plugins::block_producer
