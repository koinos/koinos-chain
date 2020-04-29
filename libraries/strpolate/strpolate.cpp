
#include <strpolate/strpolate.hpp>

#include <unordered_map>

namespace strpolate {

void strpol::to_string( std::string& result )const
{
   std::unordered_map< std::string, size_t > index;
   size_t i, n = _items.size();
   std::string key;
   bool has_key;

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
   };

   for( i=0; i<n; i++ )
   {
      index.emplace( _items[i].first, i );
   }
   n = _format_str.length();
   for( i=0; i<n; )
   {
      size_t consumed = maybe_get_key( _format_str, i, n );
      if( has_key )
      {
         auto it = index.find(key);
         if( it != index.end() )
         {
            result += _items[it->second].second;
            i += consumed;
            continue;
         }
      }
      for( size_t j=0; j<consumed; j++ )
         result += _format_str[i+j];
      i += consumed;
   }
}

}
