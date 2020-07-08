
#include <koinos/chain/exceptions.hpp>
#include <koinos/pack/classes.hpp>
#include <koinos/util.hpp>

#include <boost/filesystem.hpp>

#include <any>
#include <chrono>
#include <future>
#include <memory>

namespace koinos::chain {

KOINOS_DECLARE_DERIVED_EXCEPTION( unknown_submission_type, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( decode_exception, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( block_header_empty, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( cannot_switch_root, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( root_height_mismatch, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unknown_previous_block, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( block_height_mismatch, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( previous_id_mismatch, chain_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( invalid_signature, chain_exception );

namespace detail { class controller_impl; }

class controller
{
   public:
      controller();
      virtual ~controller();

      std::future< std::shared_ptr< types::rpc::submission_result > > submit( const types::rpc::submission_item& item );

      void open( const boost::filesystem::path& p, const std::any& o );

      void start_threads();
      void stop_threads();

      // Mock the clock for debugging
      void set_time( std::chrono::time_point< std::chrono::steady_clock > t );

   private:
      std::unique_ptr< detail::controller_impl > _my;
};

}
