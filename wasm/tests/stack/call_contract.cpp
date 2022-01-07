
#include <koinos/system/system_calls.hpp>

using namespace std::string_literals;

const std::string user_contract = "\x00\xeb\xfd\x98\x40\x48\xd9\xb6\xc8\x4b\x29\xea\x8b\x9e\xa3\x9e\x5c\x45\xeb\x92\xc0\x63\xc7\xcc\x8f"s;

int main()
{
   koinos::system::call_contract( user_contract, 0, std::string() );

   return 0;
}
