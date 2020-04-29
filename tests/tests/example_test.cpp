#include <boost/test/unit_test.hpp>

#include "../test_fixtures/example_fixture.hpp"

BOOST_FIXTURE_TEST_SUITE( example_tests, example_fixture )

BOOST_AUTO_TEST_CASE( example_test )
{
   BOOST_REQUIRE( true );
}

BOOST_AUTO_TEST_SUITE_END()
