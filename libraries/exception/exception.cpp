
#include <koinos/exception.hpp>

namespace koinos { namespace detail {

std::string json_strpolate( const std::string& format_str, const nlohmann::json& j )
{
   size_t i, n = j.size();
   std::string key;
   bool has_key;
   std::string result;

   auto maybe_get_key = [&]( const std::string& str, size_t start, size_t end ) -> size_t
   {
      // Consume at least one character
      // Return number of characters consumed
      // If key is found, set has_key, key
      has_key = false;
      size_t key_start = start+2;
      if( key_start >= end )
         return (end-start);
      if( str[start  ] != '$' )
         return 1;
      if( str[start+1] != '{' )
         return 1;
      if( str[key_start] == '$' )    // Use ${$ to escape a literal ${
         return 3;
      for( size_t i=key_start; i<end; i++ )
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

   n = format_str.length();
   for( i=0; i<n; )
   {
      size_t consumed = maybe_get_key( format_str, i, n );
      if( has_key )
      {
         auto it = j.find(key);
         if( it != j.end() )
         {
            result += it->dump();
            i += consumed;
            continue;
         }
      }
      for( size_t j=0; j<consumed; j++ )
         result += format_str[i+j];
      i += consumed;
   }

   return result;
}

json_initializer::json_initializer( nlohmann::json& j ) : _j(j) {}

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
   const_cast< std::string& >(msg) = detail::json_strpolate( msg, *boost::get_error_info< koinos::detail::json_info >( *this ) );
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

} // koinos
