#include <koinos/system/system_calls.hpp>

int main()
{
   koinos::system::db_put_object( 1, 0, koinos::system::get_contract_args() );
   return 0;
}
