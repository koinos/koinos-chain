
#include <koinos/system/system_calls.hpp>

using namespace std::string_literals;

const std::string contract_id = "\x00\x4c\x6c\x3b\xf1\x6e\x47\xa2\x54\xe3\x41\x27\x0c\x00\x6c\x01\x4d\x09\x6d\x34\x6c\xb9\xb4\x6a\xcf"s;

int main()
{
   koinos::system::call( contract_id, 0, std::string() );

   return 0;
}
