#include <boost/test/unit_test.hpp>

#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/host.hpp>
#include <koinos/chain/mempool.hpp>

#include <memory>

using namespace koinos;

struct mempool_fixture
{
   mempool_fixture()
   {
      chain::register_host_functions();
      _ctx = std::make_shared< chain::apply_context >();
      _ctx->privilege_level = chain::privilege::kernel_mode;
      _host_api = std::make_unique< chain::host_api >( *_ctx );
   }

   std::unique_ptr< chain::host_api >      _host_api;
   std::shared_ptr< chain::apply_context > _ctx;
};

BOOST_FIXTURE_TEST_SUITE( mempool_tests, mempool_fixture )

BOOST_AUTO_TEST_CASE( mempool_test )
{
   chain::mempool mempool( _ctx );
}

BOOST_AUTO_TEST_SUITE_END()
