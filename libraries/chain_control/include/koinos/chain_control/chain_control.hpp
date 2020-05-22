
#include <koinos/chain_control/submit.hpp>
#include <koinos/exception.hpp>

#include <chrono>
#include <future>
#include <memory>

#pragma message( "Move this somewhere else, please!" )
namespace koinos { namespace protocol {
   bool operator >( const block_height_type& a, const block_height_type& b );
   bool operator >=( const block_height_type& a, const block_height_type& b );
   bool operator <( const block_height_type& a, const block_height_type& b );
   bool operator <=( const block_height_type& a, const block_height_type& b );
}

namespace chain_control {

DECLARE_KOINOS_EXCEPTION( UnknownSubmitType );

class chain_controller_impl;

class chain_controller
{
   public:
      chain_controller();
      virtual ~chain_controller();

      std::future< std::shared_ptr< submit_return > > submit( const submit_item& item );

      // Mock the clock for debugging
      void set_time( std::chrono::time_point< std::chrono::steady_clock > t );

   private:
      std::unique_ptr< chain_controller_impl > _my;
};

} }
