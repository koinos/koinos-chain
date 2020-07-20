#include <thread>
#include <chrono>
#include <boost/interprocess/streams/vectorstream.hpp>
#include <boost/thread.hpp>

#include <koinos/pack/rt/json_fwd.hpp>

#include <koinos/pack/classes.hpp>
#include <koinos/plugins/block_producer/block_producer_plugin.hpp>
#include <koinos/pack/rt/binary.hpp>
#include <koinos/pack/rt/json.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/log.hpp>
#include <koinos/util.hpp>

namespace koinos::plugins::block_producer {

using namespace koinos::types;

using vectorstream = boost::interprocess::basic_vectorstream< std::vector< char > >;

static types::timestamp_type timestamp_now()
{
   auto duration = std::chrono::system_clock::now().time_since_epoch();
   auto ticks = std::chrono::duration_cast< std::chrono::milliseconds >( duration ).count();

   types::timestamp_type t;
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

   // Get previous block data
   rpc::block_topology topology;
   rpc::query_param_item p = rpc::get_head_info_params();
   vectorstream ostream;
   pack::to_binary( ostream, p );
   crypto::variable_blob query_bytes{ ostream.vector() };
   rpc::query_submission query{ query_bytes };
   auto& controller = appbase::app().get_plugin< chain::chain_plugin >().controller();
   auto r = controller.submit( rpc::submission_item( query ) );
   rpc::query_item_result q;
   try
   {
      auto w = std::get< rpc::query_submission_result >( *(r.get()) );
      vectorstream istream( w.result );
      pack::from_binary( istream, q );
      std::visit( koinos::overloaded {
         [&]( rpc::get_head_info_result& head_info ) {
            active_data.height = head_info.height + 1;
            topology.previous = head_info.id;
            topology.height = active_data.height;
         },
         []( auto& ){}
      }, q );
   }
   catch ( const std::exception &e )
   {
      LOG(error) << e.what();
   }

   // Add passive data
   block->passive_data = protocol::passive_block_data();

   //
   // +-----------+      +--------------+      +-------------------------+      +---------------------+
   // | Block sig | ---> | Block active | ---> | Transaction merkle root | ---> | Transaction actives |
   // +-----------+      +--------------+      +-------------------------+      +---------------------+
   //                           |
   //                           V
   //                +----------------------+      +----------------------+
   //                |                      | ---> |     Block passive    |
   //                |                      |      +----------------------+
   //                |                      |
   //                |                      |      +----------------------+
   //                | Passives merkle root | ---> | Transaction passives |
   //                |                      |      +----------------------+
   //                |                      |
   //                |                      |      +----------------------+
   //                |                      | ---> |   Transaction sigs   |
   //                +----------------------+      +----------------------+
   //

   std::vector< types::multihash_type > trx_active_hashes( block->transactions.size() );
   std::vector< types::multihash_type > passive_hashes( block->transactions.size() + 1 );

   // Hash transaction actives, passives, and signatures for merkle roots
   for( size_t i = 0; i < block->transactions.size(); i++ )
   {
      crypto::hash_blob( trx_active_hashes[i], CRYPTO_SHA2_256_ID, block->transactions[i]->active_data );
      crypto::hash_blob( passive_hashes[(2*i)+1], CRYPTO_SHA2_256_ID, block->transactions[i]->passive_data );
      crypto::hash_blob( passive_hashes[(2*i)+2], CRYPTO_SHA2_256_ID, block->transactions[i]->signature_data );
   }

   crypto::hash_blob( passive_hashes[0], CRYPTO_SHA2_256_ID, block->passive_data );

   crypto::merkle_hash_leaves( trx_active_hashes, CRYPTO_SHA2_256_ID );
   crypto::merkle_hash_leaves( passive_hashes, CRYPTO_SHA2_256_ID );

   active_data.header_hashes.digests.resize(3);
   active_data.header_hashes.digests[(uint32_t)protocol::header_hash_index::transaction_merkle_root_hash_index] = trx_active_hashes[0].digest;
   active_data.header_hashes.digests[(uint32_t)protocol::header_hash_index::passive_data_merkle_root_hash_index] = passive_hashes[0].digest;
   active_data.header_hashes.digests[(uint32_t)protocol::header_hash_index::previous_block_hash_index] = topology.previous.digest;
   crypto::multihash::set_id( active_data.header_hashes, CRYPTO_SHA2_256_ID );
   crypto::multihash::set_size( active_data.header_hashes, 32 );

   // Serialize active data, store it in block header
   block->active_data = active_data;

   // Hash active data and use it to sign block
   multihash_type digest;
   crypto::hash_blob( digest, CRYPTO_SHA2_256_ID, block->active_data );
   auto signature = block_signing_private_key.sign_compact( digest );
   pack::to_variable_blob( block->signature_data, signature );

   // Store hash of header as ID
   topology.id = crypto::hash( CRYPTO_SHA2_256_ID, block->active_data );

   // Create the submit block object
   rpc::block_submission block_submission;
   block_submission.topology = topology;
   block_submission.block = *block;

   // Submit the block
   rpc::submission_item si = block_submission;
   r = controller.submit( si );
   try
   {
      r.get(); // TODO: Probably should do something better here, rather than discarding the result...
   }
   catch ( const std::exception &e )
   {
      LOG(error) << e.what();
   }

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
