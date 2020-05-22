#include <thread>
#include <boost/interprocess/streams/vectorstream.hpp>

#include <koinos/chain_control/submit.hpp>
#include <koinos/plugins/block_producer/block_producer_plugin.hpp>
#include <koinos/pack/rt/binary.hpp>
#include <koinos/crypto/multihash.hpp>

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace koinos::plugins::block_producer {

   typedef boost::interprocess::basic_vectorstream< std::vector<char> > vectorstream;

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

       // Get active bytes
       protocol::active_block_data active_data;
       active_data.timestamp = timestamp_now();
       // TODO: block->active.height = get_height();
       vectorstream active_stream;
       protocol::to_binary(active_stream, active_data);
       crypto::vl_blob active_data_bytes{active_stream.vector()};
       block->active_bytes = active_data_bytes;

       protocol::passive_block_data passive_data;
       auto digest = crypto::hash(CRYPTO_SHA2_256_ID, active_data);
       auto signature = block_signing_private_key.sign_compact(digest);
       passive_data.block_signature = signature;

       auto passive_hash = crypto::hash(CRYPTO_SHA2_256_ID, passive_data);
       block->passive_merkle_root = passive_hash;
       block->active_bytes = active_data_bytes;

       pack::block_topology topology;
       // TODO: get previous and set the field

       // Serialize the active data
       //vectorstream stream;
       //protocol::to_binary(stream, block->active);
       //crypto::vl_blob active_bytes{stream.vector()};

       // Get height
       protocol::get_head_info_params p;
       vectorstream ostream;
       pack::to_binary(ostream, p);
       crypto::vl_blob query_bytes{ostream.vector()};
       pack::submit_query query{query_bytes};
       auto& controller = appbase::app().get_plugin< chain::chain_plugin >().controller();
       auto r = controller.submit(pack::submit_item(query));
       //block_height_type height;
       //multihash_type id;

       protocol::query_result_item q;
       try {
           auto w = std::get<chain_control::submit_return_query>(*(r.get()));
           vectorstream istream(w.result.data);
           pack::from_binary(istream, q);
           std::visit(overloaded{
               [&](protocol::get_head_info_return& head_info) {
                   active_data.height.height = head_info.height.height+1;
                   topology.previous = head_info.id;
                   topology.block_num = active_data.height;
               }
           },q);

       } catch (...) {
           std::cout << "no";
       }

       //pack::from_binary(istream, head_info);
       //head_info.height!!!

       //submit_item item;
       //item.q
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
