#include <koinos/system/system_calls.hpp>

#include <string>

using namespace std::string_literals;

int main()
{
   koinos::system::log( koinos::system::get_arguments().second );

   return 0;
}
