
#define BOOST_THREAD_PROVIDES_EXECUTORS
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_USES_MOVE

#include <boost/filesystem.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/interprocess/streams/vectorstream.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/sync_bounded_queue.hpp>

#include <koinos/pack/rt/string_fwd.hpp>

#include <koinos/chain/system_calls.hpp>

#include <koinos/pack/classes.hpp>
#include <koinos/pack/rt/binary.hpp>
#include <koinos/pack/rt/json.hpp>
#include <koinos/pack/rt/string.hpp>

#include <koinos/chain_control/chain_control.hpp>
#include <koinos/chain_control/submit.hpp>

#include <koinos/crypto/multihash.hpp>

#include <koinos/exception.hpp>

#include <koinos/fork/block_state.hpp>
#include <koinos/fork/fork_database.hpp>

#include <koinos/log/log.hpp>

#include <koinos/pack/classes.hpp>
#include <koinos/pack/rt/binary.hpp>
#include <koinos/pack/rt/json.hpp>

#include <koinos/statedb/statedb.hpp>

#include <chainbase/chainbase.hpp>

#include <mira/database_configuration.hpp>

#include <algorithm>
#include <chrono>
#include <list>
#include <memory>
#include <mutex>
#include <optional>

#pragma message( "Move this somewhere else, please!" )
namespace koinos { namespace protocol {

bool operator >( const block_height_type& a, const block_height_type& b )  { return a.height > b.height;  }
bool operator >=( const block_height_type& a, const block_height_type& b ) { return a.height >= b.height; }
bool operator <( const block_height_type& a, const block_height_type& b )  { return a.height < b.height;  }
bool operator <=( const block_height_type& a, const block_height_type& b ) { return a.height <= b.height; }

} // protocol

namespace chain_control {

/**
 * Represents the block in the fork DB.
 */
constexpr std::size_t MAX_QUEUE_SIZE = 1024;

using koinos::protocol::block_topology;
using koinos::protocol::block_header;
using koinos::protocol::vl_blob;
using fork_database_type = koinos::fork::fork_database< block_topology >;
using koinos::statedb::state_db;
using namespace std::string_literals;

using vectorstream = boost::interprocess::basic_vectorstream< std::vector< char > >;
std::vector< char > to_vlblob( std::string&& s ){ return std::vector< char >( s.begin(), s.end() ); }

struct submit_item_impl
{
   submit_item_impl();
   virtual ~submit_item_impl();
};

struct submit_block_impl
   : public submit_item_impl
{
   submit_block_impl( const submit_block& s ) : sub(s) {}

   submit_block             sub;

   fork_database_type::block_state_ptr topo_ptr;
   protocol::block_header   header;
   std::vector< vl_blob >   transactions;
   std::vector< vl_blob >   passives;
};

struct submit_transaction_impl
   : public submit_item_impl
{
   submit_transaction_impl( const submit_transaction& s ) : sub(s) {}

   submit_transaction   sub;
};

struct submit_query_impl
   : public submit_item_impl
{
   submit_query_impl( const submit_query& s ) : sub(s) {}

   submit_query         sub;
};

struct work_item
{
   std::shared_ptr< submit_item_impl >                 item;
   std::chrono::nanoseconds                            submit_time;
   std::chrono::nanoseconds                            work_begin_time;
   std::chrono::nanoseconds                            work_end_time;

   std::promise< std::shared_ptr< submit_return > >    prom_work_done;   // Promise set when work is done
   std::future< std::shared_ptr< submit_return > >     fut_work_done;    // Future corresponding to prom_work_done
   std::promise< std::shared_ptr< submit_return > >    prom_output;      // Promise that was returned to submit() caller
};

// Helper class for overloading variant visitors
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// We need to do some additional work, we need to index blocks by all accepted hash algorithms.

/**
 * Submission API for blocks, transactions, and queries.
 *
 * chain_controller manages the locks on the DB and fork DB.
 *
 * It knows which queries can run together based on the internal semantics of fork DB,
 * so multithreading must live in this class.
 *
 * The multithreading is CSP (Communicating Sequential Processes), as it is the easiest
 * paradigm for writing bug-free code.
 *
 * However, the state of C++ support for CSP style multithreading is rather unfortunate.
 * There is no thread-safe queue in the standard library, and the Boost sync_bounded_queue
 * class is marked as experimental.  Some quick Googling suggests that if you want a
 * thread-safe queue class in C++, the accepted practice is to "roll your own" -- ugh.
 * We'll use the sync_bounded_queue class here for now, which means we need to use Boost
 * threading internally.  Let's keep the interface based on std::future.
 */
class chain_controller_impl
{
   public:
      chain_controller_impl();
      virtual ~chain_controller_impl();

      void start_threads();
      void stop_threads();

      std::future< std::shared_ptr< submit_return > > submit( const submit_item& item );
      void set_time( std::chrono::time_point< std::chrono::steady_clock > t );

   private:
      std::shared_ptr< submit_return > process_item( std::shared_ptr< submit_item_impl > item );

      void process_submit_block( submit_return_block& ret, submit_block_impl& block );
      void process_submit_transaction( submit_return_transaction& ret, submit_transaction_impl& tx );
      void process_submit_query( submit_return_query& ret, submit_query_impl& query );

      void feed_thread_main();
      void work_thread_main();

      std::chrono::time_point< std::chrono::steady_clock > now()
      {   return (_now) ? (*_now) : std::chrono::steady_clock::now();     }

      fork_database_type                                                       _fork_db;
      state_db                                                                 _state_db;
      std::mutex                                                               _state_db_mutex;
      chain::system_call_table                                                 _syscall_table;
      std::unique_ptr< chain::system_api >                                     _sys_api;
      std::unique_ptr< chain::apply_context >                                  _ctx;
      chainbase::database                                                      _db;

      // Item lifetime:
      //
      // (submit) ---> input_queue ---> (feed_thread) ---> work_queue ---> (work_thread) ---> promise finished
      //
      // Items start in input queue.
      // Stateful processing is done by work_thread (IO-bound, not parallel).
      //
      // Feed thread contains scheduler logic, moves items that can be worked on concurrently from input queue to work queue.
      // Work threads consume the work queue and move completed work to the output queue.

      boost::concurrent::sync_bounded_queue< std::shared_ptr< work_item > >    _input_queue{ MAX_QUEUE_SIZE };
      boost::concurrent::sync_bounded_queue< std::shared_ptr< work_item > >    _work_queue{ MAX_QUEUE_SIZE };

      size_t                                                                   _thread_stack_size = 4096*1024;
      std::optional< boost::thread >                                           _feed_thread;
      std::vector< boost::thread >                                             _work_threads;
      std::optional< std::chrono::time_point< std::chrono::steady_clock > >    _now;
};

submit_item_impl::submit_item_impl()
{}

submit_item_impl::~submit_item_impl()
{}

chain_controller::chain_controller()
   : _my( std::make_unique< chain_controller_impl >() )
{}

chain_controller::~chain_controller()
{}

chain_controller_impl::chain_controller_impl()
{
   auto tmp = boost::filesystem::current_path() / boost::filesystem::unique_path();

   _db.open( tmp, 0, mira::utilities::default_database_configuration() );
//   _db.add_index< chain::table_id_multi_index >();
//   _db.add_index< chain::key_value_index >();
//   _db.add_index< chain::index64_index >();
//   _db.add_index< chain::index128_index >();
//   _db.add_index< chain::index256_index >();
//   _db.add_index< chain::index_double_index >();
//   _db.add_index< chain::index_long_double_index >();

   _ctx = std::make_unique< chain::apply_context >( _db, _syscall_table );
   _ctx->privilege_level = chain::privilege::kernel_mode;
   _sys_api = std::make_unique< chain::system_api >( *_ctx );
}

chain_controller_impl::~chain_controller_impl()
{}

std::future< std::shared_ptr< submit_return > > chain_controller::submit( const submit_item& item )
{
   return _my->submit( item );
}

void chain_controller::set_time( std::chrono::time_point< std::chrono::steady_clock > t )
{
   _my->set_time( t );
}

void chain_controller_impl::set_time( std::chrono::time_point< std::chrono::steady_clock > t )
{
   _now = t;
}

struct create_impl_item_visitor
{
   template< typename T >
   std::shared_ptr< submit_item_impl > operator()( const T& sub ) const
   {   KOINOS_THROW( UnknownSubmitType, "Unimplemented submission type" ); }

   std::shared_ptr< submit_item_impl > operator()( const submit_block& sub ) const
   {   return std::shared_ptr< submit_item_impl >( std::make_shared< submit_block_impl >( sub ) ); }

   std::shared_ptr< submit_item_impl > operator()( const submit_transaction& sub ) const
   {   return std::shared_ptr< submit_item_impl >( std::make_shared< submit_transaction_impl >( sub ) ); }

   std::shared_ptr< submit_item_impl > operator()( const submit_query& sub ) const
   {   return std::shared_ptr< submit_item_impl >( std::make_shared< submit_query_impl >( sub ) ); }
};

std::future< std::shared_ptr< submit_return > > chain_controller_impl::submit( const submit_item& item )
{
   create_impl_item_visitor vtor;
   std::shared_ptr< submit_item_impl > impl_item = std::visit( vtor, item );
   std::shared_ptr< work_item > work = std::make_shared< work_item >();
   work->item = impl_item;
   work->submit_time = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::system_clock::now().time_since_epoch() );
   work->fut_work_done = work->prom_work_done.get_future();
   std::future< std::shared_ptr< submit_return > > fut_output = work->prom_output.get_future();
   try
   {
      _input_queue.push_back( work );
   }
   catch( const boost::concurrent::sync_queue_is_closed& e )
   {
      // Do nothing.  If we're closing down queues, we still return a future for which valid() is true,
      // but wait() will block forever.  (The caller must cleanly handle the case of a future
      // whose wait() blocks forever anyway, since this may occur for items that were already
      // enqueued at the time of shutdown.)
   }
   return fut_output;
}

DECLARE_KOINOS_EXCEPTION( decode_exception );
DECLARE_KOINOS_EXCEPTION( block_header_empty );
DECLARE_KOINOS_EXCEPTION( cannot_switch_root );
DECLARE_KOINOS_EXCEPTION( root_height_mismatch );
DECLARE_KOINOS_EXCEPTION( unknown_previous_block );
DECLARE_KOINOS_EXCEPTION( block_height_mismatch );
DECLARE_KOINOS_EXCEPTION( previous_id_mismatch );
DECLARE_KOINOS_EXCEPTION( invalid_signature );

template< typename T > void decode_canonical( const vl_blob& bin, T& target )
{
   boost::interprocess::ibufferstream s( bin.data.data(), bin.data.size() );
   pack::from_binary( s, target );
   // No-padding check:  Enforce that bin doesn't have extra bytes that were unread
   KOINOS_ASSERT( size_t( s.tellg() ) == bin.data.size(), decode_exception, "Data does not deserialize (extra padding)", () );

   // Canonicity check:
   // Re-serialize the data and ensure it is the same as the input
   // The binary serialization format is intended to have a canonical serialization,
   // so if this check ever fails, there is a bug in the serialization spec / code.
   std::vector< char > tmp( bin.data.size() );
   boost::interprocess::bufferstream s2( tmp.data(), tmp.size() );

   pack::to_binary( s2, target );

   KOINOS_ASSERT( s2.good(), decode_exception, "Data does not reserialize (overflow)", () );
   KOINOS_ASSERT( size_t( s2.tellp() ) == bin.data.size(), decode_exception, "Data does not reserialize (size mismatch)", () );
   KOINOS_ASSERT( bin.data == tmp, decode_exception, "Data does not reserialize", () );
}

void decode_block( submit_block_impl& block )
{
   KOINOS_ASSERT( block.sub.block_header_bytes.data.size() >= 1, block_header_empty, "Block has empty header", () );

   decode_canonical( block.sub.block_header_bytes, block.header );

   // Deserialize submitted transactions
   size_t n_transactions = block.transactions.size();
   for( size_t i=0; i < n_transactions; i++ )
      decode_canonical( block.transactions[i], block.transactions[i] );

   size_t n_passives = block.passives.size();
   for( size_t i=0; i < n_passives; i++ )
      decode_canonical( block.passives[i], block.passives[i] );
}

inline bool multihash_is_zero( const koinos::protocol::multihash_type& mh )
{
   return std::all_of( mh.digest.data.begin(), mh.digest.data.end(),
      []( char c ) { return (c == 0); } );
}

void chain_controller_impl::process_submit_block( submit_return_block& ret, submit_block_impl& block )
{
   decode_block( block );
   block.topo_ptr = std::make_shared< fork::block_state< block_topology > >( block.sub.block_topo );

   std::lock_guard< std::mutex > lock( _state_db_mutex );
   if( multihash_is_zero( block.sub.block_topo.previous ) )
   {
      // Genesis case
      KOINOS_ASSERT( block.sub.block_topo.block_num.height == 1, root_height_mismatch, "First block must have height of 1", () );

      _fork_db.reset( block.topo_ptr );
      return;
   }

   fork_database_type::block_state_ptr maybe_previous = _fork_db.fetch_block( block.sub.block_topo.previous );

   KOINOS_ASSERT( maybe_previous, unknown_previous_block, "Unknown previous block", () );
   KOINOS_ASSERT( block.sub.block_topo.block_num.height == maybe_previous->block_num().height + 1, block_height_mismatch, "Block height must increase by 1", () );
   // Following assert should never trigger, as it could only be caused by a serious bug in fork_database or BMIC
   KOINOS_ASSERT( maybe_previous->id() == block.sub.block_topo.previous, previous_id_mismatch, "Previous block ID does not match", () );

   crypto::recoverable_signature sig;
   vectorstream in_sig( block.sub.block_passives_bytes[ 0 ].data );
   pack::from_binary( in_sig, sig );

   crypto::multihash_type digest = crypto::hash( CRYPTO_SHA2_256_ID, block.header.active_bytes.data.data(), block.header.active_bytes.data.size() );
   KOINOS_ASSERT( _sys_api->verify_block_header( sig, digest ), invalid_signature, "invalid block signature" );

#pragma message( "TODO:  Walk statedb to where it needs to be" )
   /*
   fork_database_type::branch_pair_type path = _fork_db.fetch_branch_from( _fork_db.head()->id(), block.topo.previous );

   for( fork_database_type::block_state_ptr& p : path.first )
   {
      // TODO:  Walk statedb up along path to arrive at MRCA
   }
   for( fork_database_type::block_state_ptr& p : path.second )
   {
      // TODO:  Walk statedb down along path to arrive at state
   }
   */

#pragma message( "TODO:  Apply block" )
#pragma message( "TODO:  Walk statedb back to forkdb head" )

   // Add successful block to forkdb
   _fork_db.add( block.topo_ptr );

#pragma message( "TODO:  Report success / failure to caller" )
}

void chain_controller_impl::process_submit_transaction( submit_return_transaction& ret, submit_transaction_impl& tx )
{
   std::lock_guard< std::mutex > lock( _state_db_mutex );
}

void chain_controller_impl::process_submit_query( submit_return_query& ret, submit_query_impl& query )
{
   query_param_item params;
   vectorstream in( query.sub.query.data );
   pack::from_binary( in, params );

   query_result_item result;
   std::lock_guard< std::mutex > lock( _state_db_mutex );
   std::visit(overloaded {
      [&]( get_head_info_params& p )
      {
         auto head = _fork_db.head();
         if( head )
         {
            get_head_info_return res;
            res.id = head->id();
            res.height = head->block_num();
            result = res;
         }
         else if( _fork_db.size() == 0 )
         {
            get_head_info_return res;
            #pragma message "TODO: Replace with zero hash"
            crypto::multihash::set_id( res.id, CRYPTO_SHA2_256_ID );
            crypto::multihash::set_size( res.id, 32 );
            res.id.digest.data.resize( 32 );
            memset( res.id.digest.data.data(), 0, 32 );
            res.height.height = 0;
            result = res;
         }
         else
         {
            result = query_error{ to_vlblob( "Could not find head block"s ) };
         }
      }
   }, params);

   vectorstream out;
   pack::to_binary( out, result );
   ret.result.data = out.vector();
}

std::shared_ptr< submit_return > chain_controller_impl::process_item( std::shared_ptr< submit_item_impl > item )
{
   std::shared_ptr< submit_return > result = std::make_shared< submit_return >();

   std::shared_ptr< submit_query_impl > maybe_query = std::dynamic_pointer_cast< submit_query_impl >( item );
   if( maybe_query )
   {
      result->emplace< submit_return_query >();
      process_submit_query( std::get< submit_return_query >( *result ), *maybe_query );
      return result;
   }

   std::shared_ptr< submit_transaction_impl > maybe_transaction = std::dynamic_pointer_cast< submit_transaction_impl >( item );
   if( maybe_transaction )
   {
      result->emplace< submit_return_transaction >();
      process_submit_transaction( std::get< submit_return_transaction >( *result ), *maybe_transaction );
      return result;
   }

   std::shared_ptr< submit_block_impl > maybe_block = std::dynamic_pointer_cast< submit_block_impl >( item );
   if( maybe_block )
   {
      result->emplace< submit_return_block >();
      process_submit_block( std::get< submit_return_block >( *result ), *maybe_block );
      return result;
   }

   return result;
}

void chain_controller_impl::feed_thread_main()
{
   while( true )
   {
      std::shared_ptr< work_item > work;
      try
      {
         _input_queue.pull_front( work );
         _work_queue.push_back( work );
      }
      catch( const boost::concurrent::sync_queue_is_closed& e )
      {
         break;
      }

      // This wait() effectively disables concurrent request processing, since we wait for the worker threads
      // to complete the current item before feeding the next.
      // When we decide on a concurrent scheduling strategy, we will probably want to remove this wait().
      // We will probably also want to either set prom_output.set_value() in the worker thread,
      // or a dedicated output handling thread.
      work->fut_work_done.wait();
      std::shared_ptr< submit_return > result = work->fut_work_done.get();
      work->prom_output.set_value( result );
   }
}

void chain_controller_impl::work_thread_main()
{
   while( true )
   {
      std::shared_ptr< work_item > work;
      try
      {
         _work_queue.pull_front( work );
      }
      catch( const boost::concurrent::sync_queue_is_closed& e )
      {
         break;
      }

      std::optional< std::string > maybe_err;
      std::shared_ptr< submit_return > result;

      try
      {
         result = process_item( work->item );
      }
      catch( const exception::koinos_exception& e )
      {
         maybe_err = e.to_string();
      }
      catch( const std::exception& e )
      {
         maybe_err = e.what();
      }
      catch( ... )
      {
         maybe_err = "unknown exception";
      }

      if( maybe_err )
      {
         LOG(error) << "err in work_thread: " << (*maybe_err) << std::endl;
         result = std::make_shared< submit_return >();
         result->emplace< submit_return_error >();
         std::copy( maybe_err->begin(), maybe_err->end(), std::back_inserter( std::get< submit_return_error >( *result ).error_text.data ) );
      }

      work->prom_work_done.set_value( result );
   }
}

void chain_controller::start_threads()
{
   _my->start_threads();
}

void chain_controller::stop_threads()
{
   _my->stop_threads();
}

void chain_controller_impl::start_threads()
{
   boost::thread::attributes attrs;
   attrs.set_stack_size( _thread_stack_size );

   size_t num_threads = boost::thread::hardware_concurrency()+1;
   for( size_t i=0; i<num_threads; i++ )
   {
      _work_threads.emplace_back( attrs, [this]() { work_thread_main(); } );
   }

   _feed_thread.emplace( attrs, [this]() { feed_thread_main(); } );
}

void chain_controller_impl::stop_threads()
{
   //
   // We must close the queues in order from last to first:  A later queue may be waiting on
   // a future supplied by an earlier queue.
   // If the earlier threads are still alive, these futures will eventually complete.
   // Then the later thread will wait on its queue and see close() has been called.
   //
   _work_queue.close();
   for( boost::thread& t : _work_threads )
       t.join();
   _work_threads.clear();

   _input_queue.close();
   _feed_thread->join();
   _feed_thread.reset();
}

} }
