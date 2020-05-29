
#pragma once

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#define STRPOL( format_str, bubble_list ) \
   strpolate::strpol( format_str, strpolate::string_string_list() bubble_list )

#define STRPOLATE(format_str, bubble_list) \
   STRPOL( format_str, bubble_list ).to_string()

namespace strpolate {

inline void to_string( std::string& result, const std::string& val )
{   result = val;                 }
inline void to_string( std::string& result, int val )
{   result = std::to_string(val); }
inline void to_string( std::string& result, long val )
{   result = std::to_string(val); }
inline void to_string( std::string& result, long long val )
{   result = std::to_string(val); }
inline void to_string( std::string& result, unsigned val )
{   result = std::to_string(val); }
inline void to_string( std::string& result, unsigned long val )
{   result = std::to_string(val); }
inline void to_string( std::string& result, unsigned long long val )
{   result = std::to_string(val); }
inline void to_string( std::string& result, float val )
{   result = std::to_string(val); }
inline void to_string( std::string& result, double val )
{   result = std::to_string(val); }
inline void to_string( std::string& result, long double val )
{   result = std::to_string(val); }
inline void to_string( std::string& result, const char* val )
{   result = val;                 }
template< typename T >
inline void to_string( std::string& result, const T& t )
{
   std::stringstream ss;
   ss << t;
   result = ss.str();
}

template< typename T >
std::string to_string( T t )
{
   std::string result;
   to_string( result, t );
   return result;
}

template< typename T >
std::pair< std::string, std::string > to_key_value( const std::string& first, T second )
{
   std::pair< std::string, std::string > result;
   result.first = first;
   to_string( result.second, second );
   return result;
}

class string_string_list final
{
   public:
      string_string_list() {}
      ~string_string_list() {}

      template< typename T >
      string_string_list& operator()( const std::string& k, const T& v )
      {
         _v.emplace_back(k, to_string(v));
         return *this;
      }

      string_string_list& operator()()
      {  return *this;                      }

      std::vector< std::pair< std::string, std::string > > _v;
};

class strpol
{
   public:
      strpol( const std::string& format_str,
         const string_string_list& items )
      {
         _format_str = format_str;
         _items = items._v;
      }

      strpol( const std::string& format_str,
         const std::vector< std::pair< std::string, std::string > > items )
         : _format_str(format_str), _items(items) {}

      strpol() {}

      void to_string(std::string& result)const;

      std::string to_string()const
      {
         std::string result;
         to_string(result);
         return result;
      }

      std::string _format_str;
      std::vector< std::pair< std::string, std::string > > _items;
};

}
