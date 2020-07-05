#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <boost/algorithm/string.hpp>

#include <koinos/log.hpp>

struct log_fixture{};

BOOST_FIXTURE_TEST_SUITE( log_tests, log_fixture )

BOOST_AUTO_TEST_CASE( log_color_tests )
{
   BOOST_TEST_MESSAGE( "Testing logging library with color" );
   std::stringstream stream;
   auto buf = std::cout.rdbuf();
   std::cout.rdbuf( stream.rdbuf() );

   std::vector< std::string > logtypes {
#ifndef NDEBUG
       "<\033[32mtrace\033[0m>",
       "<\033[32mdebug\033[0m>", 
#endif            
       "<\033[32minfo\033[0m>",
       "<\033[33mwarning\033[0m>",
       "<\033[31merror\033[0m>",
       "<\033[31mfatal\033[0m>",
       "<\033[31munknown\033[0m>"
   };

   auto temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
   boost::filesystem::create_directory( temp );
   koinos::initialize_logging( temp, "log_test_%3N.log" );

   LOG( trace )   << "test";
   LOG( debug )   << "test";
   LOG( info )    << "test";
   LOG( warning ) << "test";
   LOG( error )   << "test";
   LOG( fatal )   << "test";

   // We go around our macro in order to invoke an unknown log level
   BOOST_LOG_SEV(::boost::log::trivial::logger::get(), boost::log::trivial::severity_level(10))
      << boost::log::add_value("Line", __LINE__)
      << boost::log::add_value("File", boost::filesystem::path(__FILE__).filename().string()) << "test";

   auto file_path = temp / "log_test_000.log";
   std::ifstream file( file_path.string() );
   BOOST_REQUIRE( file.is_open() );

   std::vector< std::string > log_lines;
   std::string line;

   while ( std::getline( file, line ) )
      log_lines.push_back( line );

   std::string& line_one = log_lines[0];
   std::size_t pos = line_one.find( "<" );
   std::string expected_string = line_one.substr( pos );

   std::vector< std::string > results;
   std::string stream_str = stream.str();
   boost::split( results, stream_str, boost::is_any_of( "\n" )  );
   results.pop_back();

   // Setting std::cout back to normal
   std::cout.rdbuf( buf );

   // When building in release mode trace and debug are filtered out
#ifdef NDEBUG
   BOOST_REQUIRE_EQUAL( "<info>: test", expected_string );
#else
   BOOST_REQUIRE_EQUAL( "<trace>: test", expected_string );
#endif

   BOOST_REQUIRE_EQUAL( results.size(), logtypes.size() );
   for ( int i = 0; i < results.size(); i++ )
   {
      pos = results[i].find( "<" );
      std::string expected_result = results[i].substr( pos );
      BOOST_REQUIRE_EQUAL( logtypes[i] + ": test", expected_result );
   }
}

BOOST_AUTO_TEST_CASE( log_no_color_tests )
{
   BOOST_TEST_MESSAGE( "Testing logging library without color" );
   std::stringstream stream;
   auto buf = std::cout.rdbuf();
   std::cout.rdbuf( stream.rdbuf() );

   std::vector< std::string > logtypes {
#ifndef NDEBUG
       "<trace>",
       "<debug>",
#endif
       "<info>",
       "<warning>",
       "<error>",
       "<fatal>",
       "<unknown>"
   };

   auto temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
   boost::filesystem::create_directory( temp );
   koinos::initialize_logging( temp, "log_test_%3N.log", false /* no color */ );

   LOG( trace )   << "test";
   LOG( debug )   << "test";
   LOG( info )    << "test";
   LOG( warning ) << "test";
   LOG( error )   << "test";
   LOG( fatal )   << "test";

   // We go around our macro in order to invoke an unknown log level
   BOOST_LOG_SEV(::boost::log::trivial::logger::get(), boost::log::trivial::severity_level(10))
      << boost::log::add_value("Line", __LINE__)
      << boost::log::add_value("File", boost::filesystem::path(__FILE__).filename().string()) << "test";

   auto file_path = temp / "log_test_000.log";
   std::ifstream file( file_path.string() );
   BOOST_REQUIRE( file.is_open() );

   std::vector< std::string > log_lines;
   std::string line;

   while ( std::getline( file, line ) )
      log_lines.push_back( line );

   std::string& line_one = log_lines[0];
   std::size_t pos = line_one.find( "<" );
   std::string expected_string = line_one.substr( pos );

   std::vector< std::string > results;
   std::string stream_str = stream.str();
   boost::split( results, stream_str, boost::is_any_of( "\n" )  );
   results.pop_back();

   // Setting std::cout back to normal
   std::cout.rdbuf( buf );

   // When building in release mode trace and debug are filtered out
#ifdef NDEBUG
   BOOST_REQUIRE_EQUAL( "<info>: test", expected_string );
#else
   BOOST_REQUIRE_EQUAL( "<trace>: test", expected_string );
#endif

   BOOST_REQUIRE_EQUAL( results.size(), logtypes.size() );
   for ( int i = 0; i < results.size(); i++ )
   {
      pos = results[i].find( "<" );
      std::string expected_result = results[i].substr( pos );
      BOOST_REQUIRE_EQUAL( logtypes[i] + ": test", expected_result );
   }
}

BOOST_AUTO_TEST_SUITE_END()
