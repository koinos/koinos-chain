#include <koinos/system/system_calls.hpp>

using namespace koinos;

#define KOINOS_NAME     "Koinos"
#define KOINOS_SYMBOL   "KOIN"
#define KOINOS_DECIMALS 8

#define SUPPLY_KEY uint64_t( 0 )

enum entries : uint32_t
{
   name_entry         = 0x76ea4297,
   symbol_entry       = 0x7e794b24,
   decimals_entry     = 0x59dc15ce,
   total_supply_entry = 0xcf2e8212,
   balance_of_entry   = 0x15619248,
   transfer_entry     = 0x62efa292,
   mint_entry         = 0xc2f82bdc
};

struct transfer_args
{
   chain::account_type from;
   chain::account_type to;
   uint64_t            value;
};

KOINOS_REFLECT( transfer_args, (from)(to)(value) );

struct mint_args
{
   chain::account_type to;
   uint64_t            value;
};

KOINOS_REFLECT( mint_args, (to)(value) );

std::string name()
{
   return KOINOS_NAME;
}

std::string symbol()
{
   return KOINOS_SYMBOL;
}

uint8_t decimals()
{
   return KOINOS_DECIMALS;
}

uint64_t total_supply()
{
   uint64_t supply = 0;
   system::db_get_object< uint64_t >( 0, SUPPLY_KEY, supply );
   return supply;
}

uint64_t balance_of( const chain::account_type& owner )
{
   uint64_t balance = 0;
   system::db_get_object< uint64_t >( 0, owner, balance );
   return balance;
}

bool transfer( const chain::account_type& from, const chain::account_type& to, const uint64_t& value )
{
   system::require_authority( from );
   auto from_balance = balance_of( from );

   if ( from_balance < value ) return false;

   from_balance -= value;
   auto to_balance = balance_of( to ) + value;

   system::db_put_object( 0, from, from_balance );
   system::db_put_object( 0, to, to_balance );

   return true;
}

bool mint( const chain::account_type& to, const uint64_t& amount )
{
   // TODO: Authorization
   auto supply = total_supply();
   auto new_supply = supply + amount;

   if ( new_supply < supply ) return false; // Overflow detected

   auto to_balance = balance_of( to ) + amount;

   system::db_put_object( 0, SUPPLY_KEY, new_supply );
   system::db_put_object( 0, to, to_balance );
   return true;
}

int main()
{
   auto entry_point = system::get_entry_point();
   auto args = system::get_contract_args();

   variable_blob return_blob;

   switch( uint32_t(entry_point) )
   {
      case entries::name_entry:
      {
         return_blob = pack::to_variable_blob( name() );
         break;
      }
      case entries::symbol_entry:
      {
         return_blob = pack::to_variable_blob( symbol() );
         break;
      }
      case entries::decimals_entry:
      {
         return_blob = pack::to_variable_blob( decimals() );
         break;
      }
      case entries::total_supply_entry:
      {
         return_blob = pack::to_variable_blob( total_supply() );
         break;
      }
      case entries::balance_of_entry:
      {
         auto owner = pack::from_variable_blob< chain::account_type >( args );
         system::print( std::string( owner.data(), owner.size() ) );
         return_blob = pack::to_variable_blob( balance_of( owner ) );
         break;
      }
      case entries::transfer_entry:
      {
         auto t_args = pack::from_variable_blob< transfer_args >( args );
         return_blob = pack::to_variable_blob( transfer( t_args.from, t_args.to, t_args.value ) );
         break;
      }
      case entries::mint_entry:
      {
         auto m_args = pack::from_variable_blob< mint_args >( args );
         system::print( std::string( m_args.to.data(), m_args.to.size() ) );
         return_blob = pack::to_variable_blob( mint( m_args.to, m_args.value ) );
         break;
      }
      default:
         system::exit_contract( 1 );
   }

   system::set_contract_return( return_blob );

   system::exit_contract( 0 );
   return 0;
}
