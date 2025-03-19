#include <chrono>
#include <cstddef>
#include <fstream>
#include <iostream>

#include <boost/program_options.hpp>

#include <koinos/chain/host_api.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/types.hpp>
#include <koinos/exception.hpp>

#include <koinos/chain/chain.pb.h>

#define HELP_OPTION     "help"
#define CONTRACT_OPTION "contract"
#define VM_OPTION       "vm"
#define LIST_VM_OPTION  "list"
#define TICKS_OPTION    "ticks"
#define RUNS_OPTION     "runs"

using namespace koinos;
using namespace std::chrono_literals;

int main( int argc, char** argv, char** envp )
{
  try
  {
    boost::program_options::options_description desc( "Koinos VM options" );

    // clang-format off
    desc.add_options()
      ( HELP_OPTION     ",h", "print usage message" )
      ( CONTRACT_OPTION ",c", boost::program_options::value< std::string >(), "the contract to run" )
      ( VM_OPTION       ",v", boost::program_options::value< std::string >()->default_value( "" ), "the VM backend to use" )
      ( TICKS_OPTION    ",t", boost::program_options::value< int64_t >()->default_value( std::numeric_limits< int64_t >::max() ), "set maximum allowed ticks" )
      ( LIST_VM_OPTION  ",l", "list available VM backends" )
      ( RUNS_OPTION     ",r", boost::program_options::value< uint64_t >()->default_value( 1 ), "set ");
    // clang-format on

    boost::program_options::variables_map vmap;
    boost::program_options::store( boost::program_options::parse_command_line( argc, argv, desc ), vmap );

    initialize_logging( "koinos_vm_driver", {}, "info" );

    if( vmap.count( HELP_OPTION ) )
    {
      std::cout << desc << std::endl;
      return EXIT_SUCCESS;
    }

    if( vmap.count( LIST_VM_OPTION ) )
    {
      std::cout << "Available VM Backend(s):";

      std::vector< std::shared_ptr< vm_manager::vm_backend > > backends = vm_manager::get_vm_backends();
      for( auto b: backends )
      {
        std::cout << "   " << b->backend_name() << std::endl;
      }
      return EXIT_SUCCESS;
    }

    if( !vmap.count( CONTRACT_OPTION ) )
    {
      std::cout << desc << std::endl;
      return EXIT_FAILURE;
    }

    std::filesystem::path contract_file{ vmap[ CONTRACT_OPTION ].as< std::string >() };
    if( contract_file.is_relative() )
      contract_file = std::filesystem::current_path() / contract_file;

    std::ifstream ifs( contract_file );
    std::string bytecode( ( std::istreambuf_iterator< char >( ifs ) ), ( std::istreambuf_iterator< char >() ) );

    std::string vm_backend_name = vmap[ VM_OPTION ].as< std::string >();

    auto vm_backend = vm_manager::get_vm_backend( vm_backend_name );
    KOINOS_ASSERT( vm_backend, koinos::chain::unknown_backend_exception, "Couldn't get VM backend" );

    vm_backend->initialize();
    LOG( info ) << "Initialized " << vm_backend->backend_name() << " VM backend";

    chain::execution_context ctx( vm_backend );

    //auto rld = chain::system_call::get_resource_limits( ctx );
    chain::resource_limit_data rld;
    rld.set_compute_bandwidth_limit( vmap[ TICKS_OPTION ].as< int64_t >() );
    auto run_limit = vmap[ RUNS_OPTION ].as< uint64_t >();

    auto start = std::chrono::steady_clock::now();

    for( uint64_t i = 0; i < run_limit; i++ )
    {
      ctx.resource_meter().set_resource_limit_data( rld );
      chain::host_api hapi( ctx );

      vm_backend->run( hapi, bytecode );

//      if( ctx.chronicler().logs().size() )
//      {
//        LOG( info ) << "Contract output:";
//        for( const auto& message: ctx.chronicler().logs() )
//          LOG( info ) << message;
//      }
    }

    auto stop = std::chrono::steady_clock::now();

    LOG( info ) << "Total Runtime: " << (stop - start) / 1.0s << "s";
  }
  catch( const koinos::exception& e )
  {
    LOG( fatal ) << boost::diagnostic_information( e );
    return EXIT_FAILURE;
  }
  catch( ... )
  {
    LOG( fatal ) << "unknown error";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
