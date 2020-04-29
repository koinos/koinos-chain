
#include <koinos/exception.hpp>

namespace koinos::exception {

koinos_exception::koinos_exception() {}
koinos_exception::~koinos_exception() {}

koinos_exception::koinos_exception( const strpolate::strpol& strpol )
   : _strpol(strpol) {}

void koinos_exception::to_string( std::string& result )const
{
   std::string rhs;
   _strpol.to_string(rhs);
   result = strpolate::strpol("${exc_name} at ${file}:${line}:  ", _strpol._items).to_string() + rhs;
   return;
}

std::string koinos_exception::to_string()const
{
   std::string result;
   to_string(result);
   return result;
}

} // koinos::exception
