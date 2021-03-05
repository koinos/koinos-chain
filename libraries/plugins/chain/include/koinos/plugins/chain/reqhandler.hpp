#pragma once

#include <koinos/exception.hpp>
#include <koinos/pack/classes.hpp>
#include <koinos/util.hpp>

#include <boost/filesystem.hpp>

#include <any>
#include <chrono>
#include <future>
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

class reqhandler
{
   public:
      reqhandler();
      virtual ~reqhandler();

      std::future< std::shared_ptr< types::rpc::submission_result > > submit( const types::rpc::submission_item& item );

      void open( const boost::filesystem::path& p, const std::any& o, bool reset = false );
      void connect( const std::string& amqp_url );

      void start_threads();
      void stop_threads();

   private:
      std::unique_ptr< detail::reqhandler_impl > _my;
};

} // koinos::plugins::chain
