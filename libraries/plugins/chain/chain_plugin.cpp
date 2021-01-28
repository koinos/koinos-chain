#include <koinos/plugins/chain/chain_plugin.hpp>
#include <koinos/plugins/chain/reqhandler.hpp>

#include <koinos/log.hpp>
#include <koinos/util.hpp>

#include <mira/database_configuration.hpp>

// Includes needed for message broker, move these when we move message_broker
#include <koinos/mq/message_broker.hpp>
#include <koinos/net/protocol/jsonrpc/request_handler.hpp>
#include <nlohmann/json.hpp>
#include <boost/container/flat_map.hpp>

namespace koinos::plugins::chain {

using namespace appbase;

namespace detail {

struct rpc_call
{
   mq::message req;
   mq::message resp;
   mq::error_code err;
};

typedef std::function< std::string( const std::string& ) > rpc_handler_func;

struct handler_table
{
   // Map (content_type, rpc_type) -> handler
   boost::container::flat_map< std::pair< std::string, std::string >, rpc_handler_func > _rpc_handler_map;

   void handle_rpc_call( rpc_call& call );
};

void handler_table::handle_rpc_call( rpc_call& call )
{
   std::pair< std::string, std::string > k( call.req.content_type, call.req.routing_key );

   if( !call.req.reply_to.has_value() )
   {
      LOG(error) << "Could not service RPC, reply_to not specified";
      return;
   }

   if( !call.req.correlation_id.has_value() )
   {
      LOG(error) << "Could not service RPC, correlation_id not specified";
      return;
   }

   call.resp.exchange = "";
   call.resp.routing_key = *call.req.reply_to;
   call.resp.content_type = call.req.content_type;
   call.resp.correlation_id = call.req.correlation_id;

   auto it = _rpc_handler_map.find( k );
   if( it == _rpc_handler_map.end() )
   {
      KOINOS_TODO( "Fallback to error handler indexed by content_type only, then fallback to default error handler" );
      LOG(error) << "Could not find RPC handler for requested content_type, routing_key";
      call.err = mq::error_code::failure;
      KOINOS_TODO( "More meaningful error message" );
      call.resp.data = "{\"error\":\"Could not find RPC handler for requested content_type, routing_key\"}";
      return;
   }
   call.resp.data = (it->second)(call.req.data);
   call.err = mq::error_code::success;
}

class rpc_mq_consumer : public std::enable_shared_from_this< rpc_mq_consumer >
{
   public:
      rpc_mq_consumer( const std::string& amqp_url );
      virtual ~rpc_mq_consumer();

      void start();
      void add_rpc_handler( const std::string& content_type, const std::string& rpc_type, rpc_handler_func handler );
      void connect_loop();
      void consume_rpc_loop(
         std::shared_ptr< mq::message_broker > broker,
         std::shared_ptr< handler_table > handlers );
      std::pair< mq::error_code, std::shared_ptr< std::thread > > connect();

      std::string _amqp_url;
      std::unique_ptr< std::thread > _connect_thread;
      std::shared_ptr< handler_table > _handlers;
};

using json = nlohmann::json;

struct abstract_handler
{
   std::function< json( const json::object_t& ) > handle_json;
};

/**
 * rpc_manager interfaces the generic rpc_mq_consumer with the specific request/response handlers for known serializations
 * (right now, just JSON)
 */
class rpc_manager
{
   public:
      template< typename Tin, typename Tout >
      void add_rpc_handler( const std::string& rpc_type, const std::string& method_name, std::function< Tout( const Tin& ) > handler );

      std::shared_ptr< rpc_mq_consumer > _consumer;
      boost::container::flat_map< std::string, std::shared_ptr< net::protocol::jsonrpc::request_handler > > _handler_map;
};

template< typename Tin, typename Tout > void rpc_manager::add_rpc_handler(
   const std::string& rpc_type,
   const std::string& method_name,
   std::function< Tout( const Tin& ) > handler
   )
{
   std::shared_ptr< net::protocol::jsonrpc::request_handler > jsonrpc_handler;

   auto it = _handler_map.find( rpc_type );
   if( it != _handler_map.end() )
      jsonrpc_handler = it->second;
   else
   {
      jsonrpc_handler = std::make_shared< net::protocol::jsonrpc::request_handler >();
      _handler_map.emplace( rpc_type, jsonrpc_handler );
   }

   jsonrpc_handler->add_method_handler( method_name, [=]( const json::object_t& j_req ) -> json
   {
      Tin req;
      from_json( j_req, req );
      Tout resp = handler( req );
      json j_resp;
      to_json( j_resp, resp );
      return j_resp;
   } );

   _consumer->add_rpc_handler( "application/json", rpc_type, [=]( const std::string& payload ) -> std::string
   {
      return jsonrpc_handler->handle( payload );
   } );

   KOINOS_TODO( "Add handler for binary protocol" );
}


void rpc_mq_consumer::start()
{
   _connect_thread = std::make_unique< std::thread >( [&]()
   {
      connect_loop();
   } );
}

std::pair< mq::error_code, std::shared_ptr< std::thread > > rpc_mq_consumer::connect()
{
   std::shared_ptr< mq::message_broker > broker = std::make_shared< mq::message_broker >();
   std::pair< mq::error_code, std::shared_ptr< std::thread > > result;

   result.first = broker->connect_to_url( _amqp_url );
   if( result.first != mq::error_code::success )
      return result;
   result.first = broker->queue_declare( "koinos_rpc_block_store_req" );
   if( result.first != mq::error_code::success )
      return result;

   std::shared_ptr< rpc_mq_consumer > sthis = shared_from_this();

   result.second = std::make_shared< std::thread >( [sthis,broker]() { sthis->consume_rpc_loop( broker, sthis->_handlers ); } );
   return result;
}

void rpc_mq_consumer::consume_rpc_loop(
   std::shared_ptr< mq::message_broker > broker,
   std::shared_ptr< handler_table > handlers )
{
   while( true )
   {
      auto result = broker->consume();
      if( result.first == mq::error_code::time_out )
         continue;
      if( result.first != mq::error_code::success )
      {
         LOG(error) << "Error in consume()";
         continue;
      }
      if( !result.second.has_value() )
      {
         LOG(error) << "consume() reported success but didn't return a message";
         continue;
      }

      rpc_call call;
      call.req = *result.second;

      handlers->handle_rpc_call( call );
   }
}


void rpc_mq_consumer::connect_loop()
{
   const static int RETRY_MIN_DELAY_MS = 1000;
   const static int RETRY_MAX_DELAY_MS = 25000;
   const static int RETRY_DELAY_PER_RETRY_MS = 2000;

   while( true )
   {
      int retry_count = 0;

      std::shared_ptr< std::thread > consumer_thread;

      while( true )
      {
         std::unique_ptr< mq::message_broker > broker;
         std::pair< mq::error_code, std::shared_ptr< std::thread > > result = connect();
         consumer_thread = result.second;
         if( result.first == mq::error_code::success )
            break;
         int delay_ms = RETRY_MIN_DELAY_MS + RETRY_DELAY_PER_RETRY_MS * retry_count;
         if( delay_ms > RETRY_MAX_DELAY_MS )
            delay_ms = RETRY_MAX_DELAY_MS;

         // TODO:  Add quit notification for clean termination
         std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
         retry_count++;
      }
      consumer_thread->join();
      // TODO:  Cleanup closed handlers
   }
}



class chain_plugin_impl
{
   public:
      chain_plugin_impl()  {}
      ~chain_plugin_impl() {}

      void write_default_database_config( bfs::path &p );

      bfs::path            state_dir;
      bfs::path            database_cfg;

      reqhandler           _reqhandler;
};

void chain_plugin_impl::write_default_database_config( bfs::path &p )
{
   LOG(info) << "writing database configuration: " << p.string();
   boost::filesystem::ofstream config_file( p, std::ios::binary );
   config_file << mira::utilities::default_database_configuration();
}

} // detail


chain_plugin::chain_plugin() : my( new detail::chain_plugin_impl() ) {}
chain_plugin::~chain_plugin(){}

bfs::path chain_plugin::state_dir() const
{
   return my->state_dir;
}

void chain_plugin::set_program_options( options_description& cli, options_description& cfg )
{
   cfg.add_options()
         ("state-dir", bpo::value<bfs::path>()->default_value("blockchain"),
            "the location of the blockchain state files (absolute path or relative to application data dir)")
         ("database-config", bpo::value<bfs::path>()->default_value("database.cfg"), "The database configuration file location")
         ("amqp", bpo::value<std::string>()->default_value("amqp://guest:guest@localhost:5672/"), "AMQP server URL")
         ;
   cli.add_options()
         ("force-open", bpo::bool_switch()->default_value(false), "force open the database, skipping the environment check")
         ;
}

void chain_plugin::plugin_initialize( const variables_map& options )
{
   my->state_dir = app().data_dir() / "blockchain";

   if( options.count("state-dir") )
   {
      auto sfd = options.at("state-dir").as<bfs::path>();
      if(sfd.is_relative())
         my->state_dir = app().data_dir() / sfd;
      else
         my->state_dir = sfd;
   }

   my->database_cfg = options.at( "database-config" ).as< bfs::path >();

   if( my->database_cfg.is_relative() )
      my->database_cfg = app().data_dir() / my->database_cfg;

   if( !bfs::exists( my->database_cfg ) )
   {
      my->write_default_database_config( my->database_cfg );
   }
}

void chain_plugin::plugin_startup()
{
   // Check for state directory, and create if necessary
   if ( !bfs::exists( my->state_dir ) ) { bfs::create_directory( my->state_dir ); }

   nlohmann::json database_config;

   try
   {
      boost::filesystem::ifstream config_file( my->database_cfg, std::ios::binary );
      config_file >> database_config;
   }
   catch ( const std::exception& e )
   {
      LOG(error) << "error while parsing database configuration: " << e.what();
      exit( EXIT_FAILURE );
   }

   try
   {
      my->_reqhandler.open( my->state_dir, database_config );
   }
   catch( std::exception& e )
   {
      LOG(error) << "error opening database: " << e.what();
      exit( EXIT_FAILURE );
   }

   my->_reqhandler.start_threads();
}

void chain_plugin::plugin_shutdown()
{
   LOG(info) << "closing chain database";
   KOINOS_TODO( "We eventually need to call close() from somewhere" )
   //my->db.close();
   my->_reqhandler.stop_threads();
   LOG(info) << "database closed successfully";
}

std::future< std::shared_ptr< koinos::types::rpc::submission_result > > chain_plugin::submit( const koinos::types::rpc::submission_item& item )
{
   return my->_reqhandler.submit( item );
}

} // namespace koinos::plugis::chain
