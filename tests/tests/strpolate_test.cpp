#include <boost/test/unit_test.hpp>

#include <strpolate/strpolate.hpp>

struct strpolate_fixture {};

BOOST_FIXTURE_TEST_SUITE( strpolate_tests, strpolate_fixture )

BOOST_AUTO_TEST_CASE( strpolate_tests )
{
   std::string my_name = "Pythagoras";
   int my_age = 2300;
   BOOST_CHECK( "My name is Pythagoras, I am 2300 years old." == STRPOLATE( "My name is ${name}, I am ${years} years old.", ("name", my_name)("years", my_age) ) );
   BOOST_CHECK( "Failure case" != STRPOLATE( "My name is ${name}, I am ${years} years old.", ("name", my_name)("years", my_age) ) );
}

BOOST_AUTO_TEST_SUITE_END()
