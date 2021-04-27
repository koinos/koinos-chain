#include <koinos/system/system_calls.hpp>

int main( int argc, char** argv )
{
   koinos::system::db_put_object( 1, 0, koinos::system::get_contract_args() );
   return 0;
}
