#include <koinos/log.hpp>
#include <string>
#include <boost/log/sinks/basic_sink_backend.hpp>
#include <boost/log/attributes/value_extraction.hpp>
#include <boost/log/attributes/attribute_value.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>

namespace koinos {

template< bool Color >
class console_sink_impl : public boost::log::sinks::basic_formatted_sink_backend< char, boost::log::sinks::synchronized_feeding >
{
   enum class color : uint8_t
   {
      green,
      yellow,
      red
   };

   static std::string colorize( const std::string& s, color c )
   {
      if constexpr ( !Color )
         return s;

      std::string val = "";

      switch ( c )
      {
         case color::green:
            val += "\033[32m";
            break;
         case color::yellow:
            val += "\033[33m";
            break;
         case color::red:
            val += "\033[31m";
            break;
      }

      val += s;
      val += "\033[0m";

      return val;
   }

public:
   static void consume( const boost::log::record_view& rec, const string_type& formatted_string )
   {
      auto level = rec[ boost::log::trivial::severity ];
      auto line  = rec.attribute_values()[ "Line" ].extract< int >();
      auto file  = rec.attribute_values()[ "File" ].extract< std::string >();
      auto ptime = rec.attribute_values()[ "TimeStamp" ].extract< boost::posix_time::ptime >().get();
      auto& s    = std::cout;

      auto time = ptime.time_of_day();
      auto date = ptime.date();

      s << date.year() << "-";
      s << std::right << std::setfill( '0' ) << std::setw( 2 ) << date.month().as_number() << "-";
      s << std::right << std::setfill( '0' ) << std::setw( 2 ) << date.day() << " ";
      s << std::right << std::setfill( '0' ) << std::setw( 2 ) << boost::date_time::absolute_value( time.hours() ) << ":";
      s << std::right << std::setfill( '0' ) << std::setw( 2 ) << boost::date_time::absolute_value( time.minutes() ) << ":";
      s << std::right << std::setfill( '0' ) << std::setw( 2 ) << boost::date_time::absolute_value( time.seconds() ) << ".";
      s << std::right << std::setfill( '0' ) << std::setw( 6 ) << boost::date_time::absolute_value( time.fractional_seconds() );

      s << " [" << file << ":" << line << "] ";
      s << "<";
      switch ( level.get() )
      {
         case boost::log::trivial::trace:
            s << colorize( "trace", color::green );
            break;
         case boost::log::trivial::debug:
            s << colorize( "debug", color::green );
            break;
         case boost::log::trivial::info:
            s << colorize( "info", color::green );
            break;
         case boost::log::trivial::warning:
            s << colorize( "warning", color::yellow );
            break;
         case boost::log::trivial::error:
            s << colorize( "error", color::red );
            break;
         case boost::log::trivial::fatal:
            s << colorize( "fatal", color::red );
            break;
         default:
            s << colorize( "unknown", color::red );
            break;
      }
      s << ">: " << formatted_string << std::endl;
   }
};

void initialize_logging( const std::filesystem::path& p, const std::string& file_pattern, bool color )
{
   using console_sink       = boost::log::sinks::synchronous_sink< console_sink_impl< false > >;
   using color_console_sink = boost::log::sinks::synchronous_sink< console_sink_impl< true > >;

   if ( color )
      boost::log::core::get()->add_sink( boost::make_shared< color_console_sink >() );
   else
      boost::log::core::get()->add_sink( boost::make_shared< console_sink >() );

   auto file_name = p.string() + "/" + file_pattern;

   boost::log::register_simple_formatter_factory< boost::log::trivial::severity_level, char >("Severity");

   // Output message to file, rotates when file reached 1mb or at midnight every day. Each log file
   // is capped at 1mb and total is 20mb
   boost::log::add_file_log (
      boost::log::keywords::file_name = file_name,
      boost::log::keywords::rotation_size = 1 * 1024 * 1024,
      boost::log::keywords::max_size = 20 * 1024 * 1024,
      boost::log::keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(0, 0, 0),
      boost::log::keywords::format = "%TimeStamp% [%File%:%Line%] <%Severity%>: %Message%",
      boost::log::keywords::auto_flush = true
   );

   boost::log::add_common_attributes();

#ifdef NDEBUG
   boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::info );
#endif
}

} // koinos
