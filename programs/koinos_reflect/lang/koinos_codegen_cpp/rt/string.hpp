#pragma once

#include <koinos/pack/rt/basetypes.hpp>
#include <koinos/pack/rt/json.hpp>

#include <string>

namespace strpolate {

template< typename T >
void to_string( std::string& result, const T& val )
{
   nlohmann::json j;
   koinos::pack::to_json( j, val );
   result = j.dump();
}

} // strpolate
