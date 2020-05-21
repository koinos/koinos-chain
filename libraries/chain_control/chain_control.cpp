
#define BOOST_THREAD_PROVIDES_EXECUTORS
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_USES_MOVE

#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/sync_bounded_queue.hpp>

#include <koinos/chain_control/chain_control.hpp>
#include <koinos/chain_control/submit.hpp>

#include <koinos/exception.hpp>

#include <koinos/fork/fork_database.hpp>

#include <koinos/pack/classes.hpp>

#include <koinos/statedb/statedb.hpp>

#include <chrono>
#include <list>
#include <mutex>
#include <optional>

namespace koinos { namespace chain_control {

/**
 * Represents the block in the fork DB.
 */

using koinos::protocol::block_topology;
using koinos::protocol::block_header;
using koinos::protocol::vl_blob;
using fork_database = koinos::fork::fork_database< block_topology >;
using koinos::statedb::StateDB;

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
   block_topology           topo;

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

      std::future< std::shared_ptr< submit_return > > submit( const submit_item& item );
      void set_time( std::chrono::time_point< std::chrono::steady_clock > t );

   private:
      std::shared_ptr< submit_return > process_item( std::shared_ptr< submit_item_impl > item );

      void process_submit_block( submit_return_block& ret, submit_block_impl& block );
      void process_submit_transaction( submit_return_transaction& ret, submit_transaction_impl& tx );
      void process_submit_query( submit_return_query& ret, submit_query_impl& query );

      void start_threads();
      void stop_threads();

      void work_thread_main();
      void output_thread_main();

      std::chrono::time_point< std::chrono::steady_clock > now()
      {   return (_now) ? (*_now) : std::chrono::steady_clock::now();     }

      fork_database                                                            _fork_db;
      StateDB                                                                  _state_db;
      std::mutex                                                               _state_db_mutex;

      // Item lifetime:
      //
      // (submit) ---> input_queue ---> (feed_thread) ---> prep_queue ---> (prep_thread) ---> work_queue ---> (work_thread) ---> promise finished
      //
      // Items start in input queue.
      // Stateless preprocessing is done by prep_thread (CPU-bound, fully parallel).
      // Stateful processing is done by work_thread (IO-bound, not parallel).
      //
      // Feed thread contains scheduler logic, moves items that can be worked on concurrently from input queue to work queue.
      // Work threads consume the work queue and move completed work to the output queue.

      boost::concurrent::sync_bounded_queue< std::shared_ptr< work_item > >    _input_queue;
      boost::concurrent::sync_bounded_queue< std::shared_ptr< work_item > >    _work_queue;

      size_t                                                                   _thread_stack_size = 4096*1024;
      std::shared_ptr< boost::thread >                                         _feed_thread;
      std::vector< boost::thread >                                             _prep_threads;
      std::vector< boost::thread >                                             _work_threads;
      std::optional< std::chrono::time_point< std::chrono::steady_clock > >    _now;
};

submit_item_impl::submit_item_impl()
{}

submit_item_impl::~submit_item_impl()
{}

chain_controller::chain_controller()
   : _my()
{}

chain_controller::~chain_controller()
{}

chain_controller_impl::chain_controller_impl()
{}

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
   {   KOINOS_THROW( UnknownSubmitType, "Unimplemented submission type" );

   std::shared_ptr< submit_item_impl > operator()( const submit_block& sub ) const
   {   return std::shared_ptr< submit_item_impl >( std::make_shared< submit_block_impl >( sub ) ); }

   std::shared_ptr< submit_item_impl > operator()( const submit_transaction& sub ) const
   {   return std::shared_ptr< submit_item_impl >( std::make_shared< submit_transaction_impl >( sub ) ); }

   std::shared_ptr< submit_item_impl > operator()( const submit_block& sub ) const
   {   return std::shared_ptr< submit_item_impl >( std::make_shared< submit_query_impl >( sub ) ); }
};

std::future< std::shared_ptr< submit_return > > chain_controller_impl::submit( const submit_item& item )
{
   create_impl_item_visitor vtor;
   std::shared_ptr< submit_item_impl > impl_item = std::visit( vtor, item );
   std::shared_ptr< work_item > work = std::make_shared< work_item >();
   work->item = impl_item;
   work->submit_time = now();
   work->fut_work_done = work->prom_work_done.get_future();
   std::future< std::shared_ptr< submit_return > > fut_output = work->prom_output.get_future();
   try
   {
      _input_queue.push_front( work );
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

struct submit_block
{
   vl_blob                             block_header_bytes;
   std::vector< vl_blob >              block_transactions_bytes;
   std::vector< vl_blob >              block_passives_bytes;
};

struct submit_transaction
{
   vl_blob                             transaction_active_bytes;
   vl_blob                             transaction_passive_bytes;
};

DECLARE_KOINOS_EXCEPTION( decode_exception );
DECLARE_KOINOS_EXCEPTION( block_header_empty );
DECLARE_KOINOS_EXCEPTION( unknown_block_version );

template< typename T > void decode_canonical( const vl_blob& bin, T& target )
{
   boost::interprocess::ibufferstream s( bin.data.data(), bin.data.size() );
   from_binary( s, target );
   // No-padding check:  Enforce that bin doesn't have extra bytes that were unread
   KOINOS_ASSERT( size_t( s.tellp() ) == bin.data.size(), decode_exception, "Data does not deserialize (extra padding)", () );

   // Canonicity check:
   // Re-serialize the data and ensure it is the same as the input
   // The binary serialization format is intended to have a canonical serialization,
   // so if this check ever fails, there is a bug in the serialization spec / code.
   std::vector< char > tmp( bin.data.size() );
   boost::interprocess::bufferstream s2( tmp.data.data(), tmp.data.size() );

   to_binary( s2, target );

   KOINOS_ASSERT( s2.good(), decode_exception, "Data does not reserialize (overflow)", () );
   KOINOS_ASSERT( size_t( s2.tellp() ) == bin.data.size(), decode_exception, "Data does not reserialize (size mismatch)", () );
   KOINOS_ASSERT( bin.data == tmp, decode_exception, "Data does not reserialize", () );
}

void decode_block( const submit_block_impl& block )
{
   KOINOS_ASSERT( block.sub.block_header_bytes.data.size() >= 1, block_header_empty, "Block has empty header", () );
   KOINOS_ASSERT( block.sub.block_header_bytes.data[0] == 1, unknown_block_version, "Unknown block version", () );

   decode_canonical( block.sub.block_header_bytes, sub.header );

   // Deserialize submitted transactions
   size_t n_transactions = block.block_transactions_bytes.size();
   block.transactions.resize( n_transactions );
   for( size_t i=0; i<n_transactions; i++ )
      decode_canonical( block.block_transactions_bytes[i], block.transactions[i] );

   size_t n_passives = block.block_passives_bytes.size();
   block.passives.resize( n_passives );
   for( size_t i=0; i<n_passives; i++ )
      decode_canonical( block.block_passives_bytes[i], block.passives[i] );
}

// The algorithm for computing block ID depends on the bytes of the 

void chain_controller_impl::process_submit_block( submit_return_block& ret, submit_block_impl& block )
{
   decode_block( sub );

   block.fork_node.id = multihash::crypto::

   std::lock_guard< std::mutex > lock( _state_db_mutex );
   _fork_db.add( block, false );

   // TODO finish method
}

void chain_controller_impl::process_submit_transaction( submit_return_transaction& ret, submit_transaction_impl& tx )
{
   std::lock_guard< std::mutex > lock( _state_db_mutex );
}

void chain_controller_impl::process_submit_query( submit_return_query& ret, submit_query_impl& query )
{
   std::lock_guard< std::mutex > lock( _state_db_mutex );
}

std::shared_ptr< submit_return > chain_controller_impl::process_item( std::shared_ptr< submit_item_impl > item )
{
   std::shared_ptr< submit_return > result = std::make_shared< submit_return >();

   std::shared_ptr< submit_query_impl > maybe_query = dynamic_pointer_cast< submit_query_impl >( item );
   if( maybe_query )
   {
      result->emplace< submit_return_query >();
      process_submit_query( std::get< submit_return_query >( result ), *maybe_query );
      return;
   }

   std::shared_ptr< submit_transaction_impl > maybe_transaction = dynamic_pointer_cast< submit_transaction_impl >( item );
   if( maybe_transaction )
   {
      result->emplace< submit_return_transaction >();
      process_submit_transaction( std::get< submit_return_transaction >( result ), *maybe_transaction );
      return;
   }

   std::shared_ptr< submit_block_impl > maybe_block = dynamic_pointer_cast< submit_block_impl >( item );
   if( maybe_block )
   {
      result->emplace< submit_return_block >();
      process_submit_block( std::get< submit_return_block >( result ), *maybe_block );
      return;
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
      catch( const koinos_exception& e )
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
         result = std::make_shared< submit_return >();
         result->emplace< submit_return_error >();
         std::copy( maybe_err->begin(), maybe_err->end(), std::back_inserter( std::get< submit_return_error >( *result ).error_text ) );
      }

      work->prom_work_done.set_value( maybe_result );
   }
}

void chain_controller_impl::start_threads()
{
   // TODO
}

void chain_controller_impl::stop_threads()
{
   // TODO
}

/*
      void start_threads();
      void stop_threads();

      void work_thread_main();
      void output_thread_main();
*/

} }
