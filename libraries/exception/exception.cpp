
#include <koinos/exception.hpp>

namespace koinos {

exception::exception() {}
exception::~exception() {}

exception::exception( const strpolate::strpol& strpol )
   : _strpol(strpol) {}

void exception::to_string( std::string& result )const
{
   std::string rhs;
   _strpol.to_string(rhs);
   result = strpolate::strpol("${exc} at ${file}:${line}:  ", _strpol._items).to_string() + rhs;
   return;
}

std::string exception::to_string()const
{
   std::string result;
   to_string(result);
   return result;
}

} // koinos::exception
