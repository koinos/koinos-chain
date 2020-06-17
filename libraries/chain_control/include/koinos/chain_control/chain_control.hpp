
#include <koinos/pack/classes.hpp>
#include <koinos/exception.hpp>
#include <koinos/util.hpp>

#include <boost/any.hpp>
#include <boost/filesystem.hpp>

#include <chrono>
#include <future>
#include <memory>

namespace koinos::chain_control {

DECLARE_KOINOS_EXCEPTION( unknown_submission_type );

class chain_controller_impl;

class chain_controller
{
   public:
      chain_controller();
      virtual ~chain_controller();

      std::future< std::shared_ptr< submit_return > > submit( const submit_item& item );

      void open( const boost::filesystem::path& p, const boost::any& o );

      void start_threads();
      void stop_threads();

      // Mock the clock for debugging
      void set_time( std::chrono::time_point< std::chrono::steady_clock > t );

   private:
      std::unique_ptr< chain_controller_impl > _my;
};

}
