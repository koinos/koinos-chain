#include <koinos/system/system_calls.hpp>

#include <string>

using namespace std::string_literals;

int main()
{
   koinos::system::detail::put_object( "\x01"s, "\x00"s, koinos::system::get_contract_args() );
   return 0;
}
