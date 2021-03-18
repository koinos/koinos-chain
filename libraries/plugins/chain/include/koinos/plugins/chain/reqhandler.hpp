#pragma once

#include <koinos/exception.hpp>
#include <koinos/mq/client.hpp>
#include <koinos/statedb/statedb_types.hpp>
#include <koinos/pack/classes.hpp>
#include <koinos/util.hpp>

#include <boost/filesystem.hpp>

#include <any>
#include <chrono>
#include <future>
#include <map>
#include <memory>

namespace koinos::plugins::chain {

namespace detail { class reqhandler_impl; }

KOINOS_DECLARE_EXCEPTION( reqhandler_exception );

KOINOS_DECLARE_DERIVED_EXCEPTION( unknown_submission_type, reqhandler_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( decode_exception, reqhandler_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( block_header_empty, reqhandler_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( cannot_switch_root, reqhandler_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( root_height_mismatch, reqhandler_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unknown_previous_block, reqhandler_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( block_height_mismatch, reqhandler_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( previous_id_mismatch, reqhandler_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( invalid_signature, reqhandler_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( mq_connection_failure, reqhandler_exception );

using genesis_data = std::map< statedb::object_key, statedb::object_value >;

#define KOINOS_STATEDB_SPACE        0
#define KOINOS_STATEDB_CHAIN_ID_KEY 0

class reqhandler
{
   public:
      reqhandler();
      virtual ~reqhandler();

      std::future< std::shared_ptr< types::rpc::submission_result > > submit( const types::rpc::submission_item& item );

      void open( const boost::filesystem::path& p, const std::any& o, const genesis_data& data, bool reset );
      void set_client( std::shared_ptr< mq::client > c );

      void start_threads();
      void stop_threads();
      void wait_for_jobs();

   private:
      std::unique_ptr< detail::reqhandler_impl > _my;
};

} // koinos::plugins::chain
