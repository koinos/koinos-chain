
#include <koinos/pack/classes.hpp>
#include <koinos/exception.hpp>
#include <koinos/util.hpp>

#include <boost/any.hpp>
#include <boost/filesystem.hpp>

#include <chrono>
#include <future>
#include <memory>

namespace koinos::chain {

DECLARE_KOINOS_EXCEPTION( unknown_submission_type );
DECLARE_KOINOS_EXCEPTION( decode_exception );
DECLARE_KOINOS_EXCEPTION( block_header_empty );
DECLARE_KOINOS_EXCEPTION( cannot_switch_root );
DECLARE_KOINOS_EXCEPTION( root_height_mismatch );
DECLARE_KOINOS_EXCEPTION( unknown_previous_block );
DECLARE_KOINOS_EXCEPTION( block_height_mismatch );
DECLARE_KOINOS_EXCEPTION( previous_id_mismatch );
DECLARE_KOINOS_EXCEPTION( invalid_signature );

namespace detail { class controller_impl; }

class controller
{
   public:
      controller();
      virtual ~controller();

      std::future< std::shared_ptr< types::rpc::submission_result > > submit( const types::rpc::submission_item& item );

      void open( const boost::filesystem::path& p, const boost::any& o );

      void start_threads();
      void stop_threads();

      // Mock the clock for debugging
      void set_time( std::chrono::time_point< std::chrono::steady_clock > t );

   private:
      std::unique_ptr< detail::controller_impl > _my;
};

}
