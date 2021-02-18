
#define BOOST_THREAD_PROVIDES_EXECUTORS
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_USES_MOVE

#include <boost/filesystem.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/interprocess/streams/vectorstream.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/sync_bounded_queue.hpp>

#include <koinos/chain/host.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/thunks.hpp>

#include <koinos/pack/classes.hpp>
#include <koinos/pack/rt/binary.hpp>
#include <koinos/pack/rt/json.hpp>

#include <koinos/plugins/chain/reqhandler.hpp>

#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>

#include <koinos/exception.hpp>

#include <koinos/log.hpp>

#include <koinos/mq/message_broker.hpp>

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

namespace koinos::plugins::chain {

constexpr std::size_t MAX_QUEUE_SIZE = 1024;

using koinos::statedb::state_db;
using json = nlohmann::json;

using namespace std::string_literals;

using vectorstream = boost::interprocess::basic_vectorstream< std::vector< char > >;

namespace detail {

using koinos::chain::host_api;
using koinos::chain::apply_context;
using koinos::chain::privilege;
using koinos::chain::thunk::apply_block;
using koinos::chain::thunk::get_head_info;

struct block_submission_impl
{
   block_submission_impl( const types::rpc::block_submission& s ) : submission( s ) {}

   types::rpc::block_submission          submission;
};

struct transaction_submission_impl
{
   transaction_submission_impl( const types::rpc::transaction_submission& s ) : submission( s ) {}

   types::rpc::transaction_submission   submission;
};

struct query_submission_impl
{
   query_submission_impl( const types::rpc::query_submission& s ) : submission( s ) {}

   types::rpc::query_submission         submission;
};

using item_submission_impl = std::variant< block_submission_impl, transaction_submission_impl, query_submission_impl >;

struct work_item
{
   std::shared_ptr< item_submission_impl >             item;
   std::chrono::nanoseconds                            submit_time;
   std::chrono::nanoseconds                            work_begin_time;
   std::chrono::nanoseconds                            work_end_time;

   std::promise< std::shared_ptr< types::rpc::submission_result > >    prom_work_done;   // Promise set when work is done
   std::future< std::shared_ptr< types::rpc::submission_result > >     fut_work_done;    // Future corresponding to prom_work_done
   std::promise< std::shared_ptr< types::rpc::submission_result > >    prom_output;      // Promise that was returned to submit() caller
};

// We need to do some additional work, we need to index blocks by all accepted hash algorithms.

/**
 * Submission API for blocks, transactions, and queries.
 *
 * reqhandler manages the locks on the DB.
 *
 * It knows which queries can run together based on the internal semantics of the DB,
 * so multithreading must live in this class.
 *
 * The multithreading is CSP (Communicating Sequential Processes), as it is the easiest
 * paradigm for writing bug-free code.
 *
 * However, the state of C++ support for CSP style multithreading is rather unfortunate.
 * There is no thread-safe queue in the standard library, and the Boost sync_bounded_queue
 * class is marked as experimental.  Some quick Googling suggests that if you want avoid open( const boost::filesystem::path& p, const std::any& o );
 * thread-safe queue class in C++, the accepted practice is to "roll your own" -- ugh.
 * We'll use the sync_bounded_queue class here for now, which means we need to use Boost
 * threading internally.  Let's keep the interface based on std::future.
 */
class reqhandler_impl
{
   public:
      reqhandler_impl();
      virtual ~reqhandler_impl();

      void start_threads();
      void stop_threads();

      std::future< std::shared_ptr< types::rpc::submission_result > > submit( const types::rpc::submission_item& item );
      void open( const boost::filesystem::path& p, const std::any& o );
      void connect( const std::string& amqp_url );

   private:
      std::shared_ptr< types::rpc::submission_result > process_item( std::shared_ptr< item_submission_impl > item );

      void process_submission( types::rpc::block_submission_result& ret,       const block_submission_impl& block );
      void process_submission( types::rpc::transaction_submission_result& ret, const transaction_submission_impl& tx );
      void process_submission( types::rpc::query_submission_result& ret,       const query_submission_impl& query );

      void feed_thread_main();
      void work_thread_main();

      std::chrono::time_point< std::chrono::steady_clock > now();

      state_db                                                                 _state_db;
      std::mutex                                                               _state_db_mutex;
      std::unique_ptr< host_api >                                              _host_api;
      std::unique_ptr< apply_context >                                         _ctx;
      mq::message_broker                                                       _publisher;

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
};

reqhandler_impl::reqhandler_impl()
{
   koinos::chain::register_host_functions();
   _ctx = std::make_unique< apply_context >();
   _ctx->privilege_level = privilege::kernel_mode;
   _host_api = std::make_unique< host_api >( *_ctx );
}

reqhandler_impl::~reqhandler_impl() = default;

std::chrono::time_point< std::chrono::steady_clock > reqhandler_impl::now()
{
   return std::chrono::steady_clock::now();
}

std::future< std::shared_ptr< types::rpc::submission_result > > reqhandler_impl::submit( const types::rpc::submission_item& item )
{
   std::shared_ptr< item_submission_impl > impl_item;

   std::visit( koinos::overloaded {
      [&]( const types::rpc::block_submission& sub )
      {
         impl_item = std::make_shared< item_submission_impl >( block_submission_impl( sub ) );
      },
      [&]( const types::rpc::transaction_submission& sub )
      {
         impl_item = std::make_shared< item_submission_impl >( transaction_submission_impl( sub ) );
      },
      [&]( const types::rpc::query_submission& sub )
      {
         impl_item = std::make_shared< item_submission_impl >( query_submission_impl( sub ) );
      },
      [&]( const auto& )
      {
         KOINOS_THROW( unknown_submission_type, "Unimplemented submission type" );
      }
   }, item );

   std::shared_ptr< work_item > work = std::make_shared< work_item >();
   work->item = impl_item;
   work->submit_time = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::system_clock::now().time_since_epoch() );
   work->fut_work_done = work->prom_work_done.get_future();
   std::future< std::shared_ptr< types::rpc::submission_result > > fut_output = work->prom_output.get_future();
   try
   {
      _input_queue.push_back( work );
   }
   catch( const boost::concurrent::sync_queue_is_closed& e )
   {
      // Do nothing. If we're closing down queues, fut_output will return valid() == true() and wait()
      // will return instantly, but calling get() will throw a std::future_error which needs to be
      // handled by the caller.
   }
   return fut_output;
}

void reqhandler_impl::open( const boost::filesystem::path& p, const std::any& o )
{
   _state_db.open( p, o );
}

void reqhandler_impl::connect( const std::string& amqp_url )
{
   if ( _publisher.connect( amqp_url ) != mq::error_code::success )
   {
      KOINOS_THROW( mq_connection_failure, "Unable to connect to MQ server" );
   }
}

void reqhandler_impl::process_submission( types::rpc::block_submission_result& ret, const block_submission_impl& block )
{
   std::lock_guard< std::mutex > lock( _state_db_mutex );
   if( crypto::multihash_is_zero( block.submission.topology.previous ) )
   {
      // Genesis case
      KOINOS_ASSERT( block.submission.topology.height == 1, root_height_mismatch, "First block must have height of 1" );
   }

   // Check if the block has already been applied
   auto block_node = _state_db.get_node( block.submission.topology.id );
   if ( block_node ) return; // Block has been applied

   LOG(info) << "Applying block - height: " << block.submission.topology.height
      << ", id: " << block.submission.topology.id;
   block_node = _state_db.create_writable_node( block.submission.topology.previous, block.submission.topology.id );
   KOINOS_ASSERT( block_node, unknown_previous_block, "Unknown previous block" );

   try
   {
      _ctx->set_state_node( block_node );

      apply_block(
         *_ctx,
         block.submission.block,
         block.submission.verify_passive_data,
         block.submission.verify_block_signature,
         block.submission.verify_transaction_signatures );
      auto output = _ctx->get_pending_console_output();

      if (output.length() > 0) { LOG(info) << output; }

      _ctx->clear_state_node();
      _state_db.finalize_node( block_node->id() );
   }
   catch( const koinos::exception& )
   {
      _state_db.discard_node( block_node->id() );
      throw;
   }

   if ( _publisher.is_connected() )
   {
      if ( _publisher.is_connected() )
      {
         json j;

         pack::to_json( j, broadcast::block_accepted {
            .topology = block.submission.topology,
            .block    = block.submission.block
         } );

         auto err = _publisher.publish( mq::message{
            .exchange     = "koinos_event",
            .routing_key  = "koinos.block.accept",
            .content_type = "application/json",
            .data         = j.dump()
         } );

         if ( err != mq::error_code::success )
         {
            LOG(error) << "failed to publish block application to message broker";
         }
      }
   }
}

void reqhandler_impl::process_submission( types::rpc::transaction_submission_result& ret, const transaction_submission_impl& tx )
{
   std::lock_guard< std::mutex > lock( _state_db_mutex );

   if ( _publisher.is_connected() )
   {
      json j;

      pack::to_json( j, broadcast::transaction_accepted {
         .topology    = tx.submission.topology,
         .transaction = tx.submission.transaction
      } );

      auto err = _publisher.publish( mq::message {
         .exchange     = "koinos_event",
         .routing_key  = "koinos.transaction.accept",
         .content_type = "application/json",
         .data         = j.dump()
      } );

      if ( err != mq::error_code::success )
      {
         LOG(error) << "failed to publish block application to message broker";
      }
   }
}

void reqhandler_impl::process_submission( types::rpc::query_submission_result& ret, const query_submission_impl& query )
{
   query.submission.unbox();
   ret.make_mutable();

   std::lock_guard< std::mutex > lock( _state_db_mutex );
   std::visit( koinos::overloaded {
      [&]( const types::rpc::get_head_info_params& p )
      {
         try
         {
            _ctx->set_state_node( _state_db.get_head() );
            auto head_info = get_head_info( *_ctx );
            ret = types::rpc::query_submission_result(
               rpc::chain::get_head_info_response {
                  .id = head_info.id,
                  .height = head_info.height
               }
            );
         }
         catch ( const koinos::chain::database_exception& e )
         {
            types::rpc::query_error err;
            std::string err_msg = "Could not find head block";
            std::copy( err_msg.begin(), err_msg.end(), std::back_inserter( err.error_text ) );
            ret = types::rpc::query_submission_result( std::move( err ) );
         }
      },
      [&]( const types::rpc::get_chain_id_params& p )
      {
         std::string chain_id = "koinos";
         types::rpc::get_chain_id_result cir {
            .chain_id = crypto::hash_str( CRYPTO_SHA2_256_ID, chain_id.data(), chain_id.size() )
         };
         ret = types::rpc::query_submission_result( std::move( cir ) );
      },
      [&]( const auto& )
      {
         types::rpc::query_error err;
         std::string err_msg = "Unimplemented query type";
         std::copy( err_msg.begin(), err_msg.end(), std::back_inserter( err.error_text ) );
         ret = types::rpc::query_submission_result( std::move( err ) );
      }
   }, query.submission.get_const_native() );

   ret.make_immutable();
}

std::shared_ptr< types::rpc::submission_result > reqhandler_impl::process_item( std::shared_ptr< item_submission_impl > item )
{
   types::rpc::submission_result result;

   std::visit( koinos::overloaded {
      [&]( query_submission_impl& s )
      {
         types::rpc::query_submission_result qres;
         process_submission( qres, s );
         result.emplace< types::rpc::query_submission_result >( std::move( qres ) );
      },
      [&]( transaction_submission_impl& s )
      {
         types::rpc::transaction_submission_result tres;
         process_submission( tres, s );
         result.emplace< types::rpc::transaction_submission_result >( std::move( tres ) );
      },
      [&]( block_submission_impl& s )
      {
         types::rpc::block_submission_result bres;
         process_submission( bres, s );
         result.emplace< types::rpc::block_submission_result >( std::move( bres ) );
      }
   }, *item );

   return std::make_shared< types::rpc::submission_result >( result );
}

void reqhandler_impl::feed_thread_main()
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
      std::shared_ptr< types::rpc::submission_result > result = work->fut_work_done.get();
      work->prom_output.set_value( result );
   }
}

void reqhandler_impl::work_thread_main()
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
      std::shared_ptr< types::rpc::submission_result > result;

      try
      {
         result = process_item( work->item );
      }
      catch( const koinos::exception& e )
      {
         maybe_err = e.what();
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
         LOG(error) << "err in work_thread: " << (*maybe_err);
         result = std::make_shared< types::rpc::submission_result >();
         result->emplace< types::rpc::submission_error_result >();
         std::copy( maybe_err->begin(), maybe_err->end(), std::back_inserter( std::get< types::rpc::submission_error_result >( *result ).error_text ) );
      }

      work->prom_work_done.set_value( result );
   }
}

void reqhandler_impl::start_threads()
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

void reqhandler_impl::stop_threads()
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

reqhandler::reqhandler() : _my( std::make_unique< detail::reqhandler_impl >() ) {}

reqhandler::~reqhandler() = default;

std::future< std::shared_ptr< types::rpc::submission_result > > reqhandler::submit( const types::rpc::submission_item& item )
{
   return _my->submit( item );
}

void reqhandler::open( const boost::filesystem::path& p, const std::any& o )
{
   _my->open( p, o );
}

void reqhandler::connect( const std::string& amqp_url )
{
   _my->connect( amqp_url );
}

void reqhandler::start_threads()
{
   _my->start_threads();
}

void reqhandler::stop_threads()
{
   _my->stop_threads();
}

} // koinos::plugins::chain

