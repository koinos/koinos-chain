
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

#include <koinos/chain/host.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/thunks.hpp>

#include <koinos/pack/classes.hpp>
#include <koinos/pack/rt/binary.hpp>
#include <koinos/pack/rt/json.hpp>
#include <koinos/pack/rt/string.hpp>

#include <koinos/chain/controller.hpp>

#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>

#include <koinos/exception.hpp>

#include <koinos/log.hpp>

#include <koinos/pack/classes.hpp>
#include <koinos/pack/rt/binary.hpp>
#include <koinos/pack/rt/json.hpp>

#include <koinos/statedb/statedb.hpp>

#include <koinos/util.hpp>

#include <mira/database_configuration.hpp>

#include <algorithm>
#include <chrono>
#include <list>
#include <memory>
#include <mutex>
#include <optional>

namespace koinos::chain {

constexpr std::size_t MAX_QUEUE_SIZE = 1024;

using koinos::statedb::state_db;

using namespace std::string_literals;
using namespace koinos::types;

using vectorstream = boost::interprocess::basic_vectorstream< std::vector< char > >;

namespace detail {

struct block_submission_impl
{
   block_submission_impl( const rpc::block_submission& s ) : submission( s ) {}

   rpc::block_submission          submission;

   protocol::block_header         header;
   std::vector< variable_blob >   transactions;
   std::vector< variable_blob >   passives;
};

struct transaction_submission_impl
{
   transaction_submission_impl( const rpc::transaction_submission& s ) : submission( s ) {}

   rpc::transaction_submission   submission;
};

struct query_submission_impl
{
   query_submission_impl( const rpc::query_submission& s ) : submission( s ) {}

   rpc::query_submission         submission;
};

using item_submission_impl = std::variant< block_submission_impl, transaction_submission_impl, query_submission_impl >;

struct work_item
{
   std::shared_ptr< item_submission_impl >             item;
   std::chrono::nanoseconds                            submit_time;
   std::chrono::nanoseconds                            work_begin_time;
   std::chrono::nanoseconds                            work_end_time;

   std::promise< std::shared_ptr< rpc::submission_result > >    prom_work_done;   // Promise set when work is done
   std::future< std::shared_ptr< rpc::submission_result > >     fut_work_done;    // Future corresponding to prom_work_done
   std::promise< std::shared_ptr< rpc::submission_result > >    prom_output;      // Promise that was returned to submit() caller
};

// We need to do some additional work, we need to index blocks by all accepted hash algorithms.

/**
 * Submission API for blocks, transactions, and queries.
 *
 * chain_controller manages the locks on the DB.
 *
 * It knows which queries can run together based on the internal semantics of the DB,
 * so multithreading must live in this class.
 *
 * The multithreading is CSP (Communicating Sequential Processes), as it is the easiest
 * paradigm for writing bug-free code.
 *
 * However, the state of C++ support for CSP style multithreading is rather unfortunate.
 * There is no thread-safe queue in the standard library, and the Boost sync_bounded_queue
 * class is marked as experimental.  Some quick Googling suggests that if you want avoid open( const boost::filesystem::path& p, const boost::any& o );
 * thread-safe queue class in C++, the accepted practice is to "roll your own" -- ugh.
 * We'll use the sync_bounded_queue class here for now, which means we need to use Boost
 * threading internally.  Let's keep the interface based on std::future.
 */
class controller_impl
{
   public:
      controller_impl();
      virtual ~controller_impl();

      void start_threads();
      void stop_threads();

      std::future< std::shared_ptr< rpc::submission_result > > submit( const rpc::submission_item& item );
      void open( const boost::filesystem::path& p, const boost::any& o );
      void set_time( std::chrono::time_point< std::chrono::steady_clock > t );

   private:
      std::shared_ptr< rpc::submission_result > process_item( std::shared_ptr< item_submission_impl > item );

      void process_submission( rpc::block_submission_result& ret,       block_submission_impl& block );
      void process_submission( rpc::transaction_submission_result& ret, transaction_submission_impl& tx );
      void process_submission( rpc::query_submission_result& ret,       query_submission_impl& query );

      void feed_thread_main();
      void work_thread_main();

      std::chrono::time_point< std::chrono::steady_clock > now();

      state_db                                                                 _state_db;
      std::mutex                                                               _state_db_mutex;
      std::unique_ptr< chain::host_api >                                       _host_api;
      std::unique_ptr< chain::apply_context >                                  _ctx;

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

controller_impl::controller_impl()
{
   koinos::chain::register_host_functions();
   _ctx = std::make_unique< chain::apply_context >();
   _ctx->privilege_level = chain::privilege::kernel_mode;
   _host_api = std::make_unique< chain::host_api >( *_ctx );
}

controller_impl::~controller_impl() = default;

std::chrono::time_point< std::chrono::steady_clock > controller_impl::now()
{
   return (_now) ? (*_now) : std::chrono::steady_clock::now();
}

void controller_impl::set_time( std::chrono::time_point< std::chrono::steady_clock > t )
{
   _now = t;
}

struct create_impl_item_visitor
{
   template< typename T >
   item_submission_impl operator()( const T& sub ) const
   {
      KOINOS_THROW( unknown_submission_type, "Unimplemented submission type" );
   }

   item_submission_impl operator()( const rpc::block_submission& sub ) const
   {
      return block_submission_impl( sub );
   }

   item_submission_impl operator()( const rpc::transaction_submission& sub ) const
   {
      return transaction_submission_impl( sub );
   }

   item_submission_impl operator()( const rpc::query_submission& sub ) const
   {
      return query_submission_impl( sub );
   }
};

std::future< std::shared_ptr< rpc::submission_result > > controller_impl::submit( const rpc::submission_item& item )
{
   create_impl_item_visitor vtor;
   std::shared_ptr< item_submission_impl > impl_item = std::make_shared< item_submission_impl >( std::visit( vtor, item ) );
   std::shared_ptr< work_item > work = std::make_shared< work_item >();
   work->item = impl_item;
   work->submit_time = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::system_clock::now().time_since_epoch() );
   work->fut_work_done = work->prom_work_done.get_future();
   std::future< std::shared_ptr< rpc::submission_result > > fut_output = work->prom_output.get_future();
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

void controller_impl::open( const boost::filesystem::path& p, const boost::any& o )
{
   _state_db.open( p, o );
}

template< typename T > void decode_canonical( const variable_blob& bin, T& target )
{
   boost::interprocess::ibufferstream s( bin.data(), bin.size() );
   pack::from_binary( s, target );
   // No-padding check:  Enforce that bin doesn't have extra bytes that were unread
   KOINOS_ASSERT( size_t( s.tellg() ) == bin.size(), decode_exception, "Data does not deserialize (extra padding)", () );

   // Canonicity check:
   // Re-serialize the data and ensure it is the same as the input
   // The binary serialization format is intended to have a canonical serialization,
   // so if this check ever fails, there is a bug in the serialization spec / code.
   std::vector< char > tmp( bin.size() );
   boost::interprocess::bufferstream s2( tmp.data(), tmp.size() );

   pack::to_binary( s2, target );

   KOINOS_ASSERT( s2.good(), decode_exception, "Data does not reserialize (overflow)", () );
   KOINOS_ASSERT( size_t( s2.tellp() ) == bin.size(), decode_exception, "Data does not reserialize (size mismatch)", () );
   KOINOS_ASSERT( bin == tmp, decode_exception, "Data does not reserialize", () );
}

void decode_block( block_submission_impl& block )
{
   KOINOS_ASSERT( block.submission.header_bytes.size() >= 1, block_header_empty, "Block has empty header", () );

   decode_canonical( block.submission.header_bytes, block.header );

   // Deserialize submitted transactions
   std::size_t n_transactions = block.transactions.size();
   for( std::size_t i = 0; i < n_transactions; i++ )
      decode_canonical( block.transactions[i], block.transactions[i] );

   std::size_t n_passives = block.passives.size();
   for( std::size_t i = 0; i < n_passives; i++ )
      decode_canonical( block.passives[i], block.passives[i] );
}

void controller_impl::process_submission( rpc::block_submission_result& ret, block_submission_impl& block )
{
   decode_block( block );

   std::lock_guard< std::mutex > lock( _state_db_mutex );
   if( crypto::multihash::is_zero( block.submission.topology.previous ) )
   {
      // Genesis case
      KOINOS_ASSERT( block.submission.topology.height == 1, root_height_mismatch, "First block must have height of 1", () );
   }

   auto block_node = _state_db.create_writable_node( block.submission.topology.previous, block.submission.topology.id );
   KOINOS_ASSERT( block_node, unknown_previous_block, "Unknown previous block", () );

   _ctx->set_state_node( block_node );

   crypto::recoverable_signature sig;
   vectorstream in_sig( block.submission.passives_bytes[ 0 ] );
   pack::from_binary( in_sig, sig );

   crypto::multihash_type digest = crypto::hash_str( CRYPTO_SHA2_256_ID, block.header.active_bytes.data(), block.header.active_bytes.size() );
   KOINOS_ASSERT( chain::thunk::verify_block_header( *_ctx, sig, digest ), invalid_signature, "invalid block signature" );

   thunk::apply_block( *_ctx, pack::from_variable_blob< protocol::active_block_data >( block.header.active_bytes ) );
   auto output = _ctx->get_pending_console_output();

   if (output.length() > 0) { LOG(info) << output; }

   _ctx->clear_state_node();
   _state_db.finalize_node( block_node->id() );

   KOINOS_TODO( "Report success / failure to caller" )
}

void controller_impl::process_submission( rpc::transaction_submission_result& ret, transaction_submission_impl& tx )
{
   std::lock_guard< std::mutex > lock( _state_db_mutex );
}

void controller_impl::process_submission( rpc::query_submission_result& ret, query_submission_impl& query )
{
   rpc::query_param_item params;
   vectorstream in( query.submission.query );
   pack::from_binary( in, params );

   rpc::query_item_result result;
   std::lock_guard< std::mutex > lock( _state_db_mutex );
   std::visit( koinos::overloaded {
      [&]( rpc::get_head_info_params& p )
      {
         auto head = _state_db.get_head();
         if( head )
         {
            rpc::get_head_info_result res;
            res.id = head->id();
            res.height = head->revision();
            result = res;
         }
         else
         {
            result = rpc::query_error{ pack::to_variable_blob( "Could not find head block"s ) };
         }
      }
   }, params);

   vectorstream out;
   pack::to_binary( out, result );
   ret.result = out.vector();
}

std::shared_ptr< rpc::submission_result > controller_impl::process_item( std::shared_ptr< item_submission_impl > item )
{
   rpc::submission_result result;

   std::visit( koinos::overloaded {
      [&]( query_submission_impl& s )
      {
         rpc::query_submission_result qres;
         process_submission( qres, s );
         result.emplace< rpc::query_submission_result >( std::move( qres ) );
      },
      [&]( transaction_submission_impl& s )
      {
         rpc::transaction_submission_result tres;
         process_submission( tres, s );
         result.emplace< rpc::transaction_submission_result >( std::move( tres ) );
      },
      [&]( block_submission_impl& s )
      {
         rpc::block_submission_result bres;
         process_submission( bres, s );
         result.emplace< rpc::block_submission_result >( std::move( bres ) );
      }
   }, *item );

   return std::make_shared< rpc::submission_result >( result );
}

void controller_impl::feed_thread_main()
{
   while ( true )
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
      std::shared_ptr< rpc::submission_result > result = work->fut_work_done.get();
      work->prom_output.set_value( result );
   }
}

void controller_impl::work_thread_main()
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
      std::shared_ptr< rpc::submission_result > result;

      try
      {
         result = process_item( work->item );
      }
      catch( const koinos::exception& e )
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
         result = std::make_shared< rpc::submission_result >();
         result->emplace< rpc::submission_error_result >();
         std::copy( maybe_err->begin(), maybe_err->end(), std::back_inserter( std::get< rpc::submission_error_result >( *result ).error_text ) );
      }

      work->prom_work_done.set_value( result );
   }
}

void controller_impl::start_threads()
{
   boost::thread::attributes attrs;
   attrs.set_stack_size( _thread_stack_size );

   std::size_t num_threads = boost::thread::hardware_concurrency()+1;
   for( std::size_t i = 0; i < num_threads; i++ )
   {
      _work_threads.emplace_back( attrs, [this]() { work_thread_main(); } );
   }

   _feed_thread.emplace( attrs, [this]() { feed_thread_main(); } );
}

void controller_impl::stop_threads()
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

} // detail

controller::controller() : _my( std::make_unique< detail::controller_impl >() ) {}

controller::~controller() = default;

std::future< std::shared_ptr< rpc::submission_result > > controller::submit( const rpc::submission_item& item )
{
   return _my->submit( item );
}

void controller::open( const boost::filesystem::path& p, const boost::any& o )
{
   _my->open( p, o );
}

void controller::set_time( std::chrono::time_point< std::chrono::steady_clock > t )
{
   _my->set_time( t );
}

void controller::start_threads()
{
   _my->start_threads();
}

void controller::stop_threads()
{
   _my->stop_threads();
}

} // koinos::chain
