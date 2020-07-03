
#include <koinos/exception.hpp>

namespace koinos { namespace detail {

// nlohmann still has quotes around strings in json. For prettification, we want to
// remove those when replacing in a format string.
std::string trim_quotes( std::string&& s )
{
   if( s.size() >= 2 )
   {
      if( *(s.begin())  == '"' ) s.erase( 0, 1 );
      if( *(s.rbegin()) == '"' ) s.erase( s.size() - 1, s.size() );
   }

   return s;
}

std::string json_strpolate( const std::string& format_str, const nlohmann::json& j )
{
   std::string key;
   bool has_key;
   std::string result;

   auto maybe_get_key = [&]( const std::string& str, std::size_t start, std::size_t end ) -> std::size_t
   {
      // Consume at least one character
      // Return number of characters consumed
      // If key is found, set has_key, key
      has_key = false;
      std::size_t key_start = start+2;
      if( key_start >= end )
         return (end-start);
      if( str[start  ] != '$' )
         return 1;
      if( str[start+1] != '{' )
         return 1;
      if( str[key_start] == '$' )    // Use ${$ to escape a literal ${
         return 3;
      for( std::size_t i = key_start; i < end; i++ )
      {
         if( str[i] == '}' )
         {
            has_key = true;
            key = str.substr(key_start, i-key_start);
            return (i-start)+1;
         }
      }
      return 0;
   };

   std::size_t n = format_str.length();
   for( std::size_t i = 0; i < n; )
   {
      std::size_t consumed = maybe_get_key( format_str, i, n );
      if( has_key )
      {
         auto it = j.find(key);
         if( it != j.end() )
         {
            result += trim_quotes(it->dump());
            i += consumed;
            continue;
         }
      }
      for( std::size_t j = 0; j < consumed; j++ )
         result += format_str[i+j];
      i += consumed;
   }

   return result;
}

json_initializer::json_initializer( exception& e ) :
   _e(e),
   _j(*boost::get_error_info< koinos::detail::json_info >(e))
{}

json_initializer& json_initializer::operator()( const std::string& key, const char* c )
{
   _j[key] = c;
   _e.do_message_substitution();
   return *this;
}

json_initializer& json_initializer::operator()( const std::string& key, size_t v )
{
   koinos::pack::to_json( _j[key], (uint64_t)v );
   _e.do_message_substitution();
   return *this;
}

json_initializer& json_initializer::operator()()
{
   return *this;
}

} // detail

exception::exception() { *this << koinos::detail::json_info( nlohmann::json() ); }

exception::exception( const std::string& m ) : exception() { msg = m; }

exception::exception( std::string&& m ) : exception() { msg = m; }

exception::~exception() {}

const char* exception::what() const noexcept
{
   return msg.c_str();
}

std::string exception::get_stacktrace() const
{
   std::stringstream ss;
   ss << *boost::get_error_info< koinos::detail::exception_stacktrace >( *this );
   return ss.str();
}

const nlohmann::json& exception::get_json() const
{
   return *boost::get_error_info< koinos::detail::json_info >( *this );
}

const std::string& exception::get_message() const
{
   return msg;
}

void exception::do_message_substitution()
{
   msg = detail::json_strpolate( msg, *boost::get_error_info< koinos::detail::json_info >( *this ) );
}

} // koinos
