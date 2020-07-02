#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <boost/algorithm/string.hpp>

#include <koinos/log.hpp>

struct log_fixture
{
};

BOOST_FIXTURE_TEST_SUITE( log_tests, log_fixture )

BOOST_AUTO_TEST_CASE( log_tests )
{
   std::stringstream stream;
   auto buf = std::cout.rdbuf();
   std::cout.rdbuf( stream.rdbuf() );
   std::vector< std::string > logtypes{
#ifndef NDEBUG
       "<\033[32mtrace\033[0m>",
       "<\033[32mdebug\033[0m>", 
#endif            
       "<\033[32minfo\033[0m>",
       "<\033[33mwarning\033[0m>",
       "<\033[31merror\033[0m>",
       "<\033[31mfatal\033[0m>"};


   auto temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
   boost::filesystem::create_directory( temp );
   koinos::initialize_logging( temp, "log_test_%3N.log" );
   LOG( trace ) << "test";
   LOG( debug ) << "test";   
   LOG( info ) << "test";
   LOG( warning ) << "test";
   LOG( error ) << "test";
   LOG( fatal ) << "test";

   auto file_path = temp / "log_test_000.log";
   std::ifstream file( file_path.string() );
   BOOST_REQUIRE( file.is_open() );

   std::vector< std::string > log_lines;
   std::string line;
   while ( std::getline( file, line ) )
   {
      log_lines.push_back( line );
   }
   std::string& line_one = log_lines[0];
   auto pos = line_one.find( "<" );
   std::string expected_string = line_one.substr( pos );

   std::vector< std::string > results;
   auto stream_str = stream.str();
   boost::split( results, stream_str, boost::is_any_of( "\n" )  );
   results.pop_back();

   auto pos2 = stream.str().find( "<" );
   std::string expected_string2 = stream.str().substr( pos );

   // setting std out back to normal
   std::cout.rdbuf( buf );
   // when building in release mode trace and debug are filtered out
#ifdef NDEBUG
   BOOST_REQUIRE_EQUAL( "<info>: test", expected_string );
#else
   BOOST_REQUIRE_EQUAL( "<trace>: test", expected_string );
#endif
   BOOST_REQUIRE_EQUAL( results.size(), logtypes.size() );
   for ( int i = 0; i < results.size(); i++ )
   {
      auto pos2 = results[i].find( "<" );
      std::string expected_string2 = results[i].substr( pos );
      BOOST_REQUIRE_EQUAL( logtypes[i] + ": test", expected_string2 );
   }
}

BOOST_AUTO_TEST_SUITE_END()
