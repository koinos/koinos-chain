#include <boost/program_options.hpp>

#include <koinos/exception.hpp>
#include <koinos/log.hpp>

int main( int argc, char** argv )
{
   try
   {
      boost::program_options::options_description options;
      
      return EXIT_SUCCESS;
   }
   catch ( const boost::exception& e )
   {
      LOG(fatal) << boost::diagnostic_information( e ) << std::endl;
   }
   catch ( const std::exception& e )
   {
      LOG(fatal) << e.what() << std::endl;
   }
   catch ( ... )
   {
      LOG(fatal) << "unknown exception" << std::endl;
   }

   return EXIT_FAILURE;
}
