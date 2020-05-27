#include <appbase/application.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio/signal_set.hpp>

#include <iostream>
#include <fstream>

namespace appbase {

namespace bpo = boost::program_options;
using bpo::options_description;
using bpo::variables_map;
using std::cout;

   BLARG
      set_program_options();
      bpo::store( bpo::parse_command_line( argc, argv, my->_app_options ), my->_args );

      if( my->_args.count( "help" ) ) {
         cout << my->_app_options << "\n";
         return false;
      }

      if( my->_args.count( "version" ) )
      {
         cout << version_info << "\n";
         return false;
      }

      bfs::path data_dir;
      if( my->_args.count("data-dir") )
      {
         data_dir = my->_args["data-dir"].as<bfs::path>();
         if( data_dir.is_relative() )
            data_dir = bfs::current_path() / data_dir;
      }
      else
      {
#ifdef WIN32
         char* parent = getenv( "APPDATA" );
#else
         char* parent = getenv( "HOME" );
#endif

         if( parent != nullptr )
         {
            data_dir = std::string( parent );
         }
         else
         {
            data_dir = bfs::current_path();
         }

         std::stringstream app_dir;
         app_dir << '.' << app_name;

         data_dir = data_dir / app_dir.str();

         #pragma message( "TODO: Remove this check for Steem release 0.20.1+" )
         bfs::path old_dir = bfs::current_path() / "witness_node_data_dir";
         if( bfs::exists( old_dir ) )
         {
            std::cerr << "The default data directory is now '" << data_dir.string() << "' instead of '" << old_dir.string() << "'.\n";
            std::cerr << "Please move your data directory to '" << data_dir.string() << "' or specify '--data-dir=" << old_dir.string() <<
               "' to continue using the current data directory.\n";
            exit(1);
         }
      }
      my->_data_dir = data_dir;

      bfs::path config_file_name = data_dir / "config.ini";
      if( my->_args.count( "config" ) ) {
         auto config_file_name = my->_args["config"].as<bfs::path>();
         if( config_file_name.is_relative() )
            config_file_name = data_dir / config_file_name;
      }

      if(!bfs::exists(config_file_name)) {
         write_default_config(config_file_name);
      }

      bpo::store(bpo::parse_config_file< char >( config_file_name.make_preferred().string().c_str(),
                                             my->_cfg_options, true ), my->_args );

      if(my->_args.count("plugin") > 0)
      {
         auto plugins = my->_args.at("plugin").as<std::vector<std::string>>();
         for(auto& arg : plugins)
         {
            vector<string> names;
            boost::split(names, arg, boost::is_any_of(" \t,"));
            for(const std::string& name : names)
               get_plugin(name).initialize(my->_args);
         }
      }
      for (const auto& plugin : autostart_plugins)
         if (plugin != nullptr && plugin->get_state() == abstract_plugin::registered)
            plugin->initialize(my->_args);

      bpo::notify(my->_args);

      return true;
   }
   catch (const boost::program_options::error& e)
   {
      std::cerr << "Error parsing command line: " << e.what() << "\n";
      return false;
   }
}

void application::shutdown() {
   for(auto ritr = running_plugins.rbegin();
       ritr != running_plugins.rend(); ++ritr) {
      (*ritr)->shutdown();
   }
   for(auto ritr = running_plugins.rbegin();
       ritr != running_plugins.rend(); ++ritr) {
      plugins.erase((*ritr)->get_name());
   }
   running_plugins.clear();
   initialized_plugins.clear();
   plugins.clear();
}

void application::quit() {
   io_serv->stop();
}

void application::exec() {
   /** To avoid killing process by broken pipe and continue regular app shutdown.
    *  Useful for usecase: `steemd | tee steemd.log` and pressing Ctrl+C
    **/
   signal(SIGPIPE, SIG_IGN);

   std::shared_ptr<boost::asio::signal_set> sigint_set(new boost::asio::signal_set(*io_serv, SIGINT));
   sigint_set->async_wait([sigint_set,this](const boost::system::error_code& err, int num) {
     writer( "caught SIGINT" );
     quit();
     sigint_set->cancel();
   });

   std::shared_ptr<boost::asio::signal_set> sigterm_set(new boost::asio::signal_set(*io_serv, SIGTERM));
   sigterm_set->async_wait([sigterm_set,this](const boost::system::error_code& err, int num) {
     writer( "caught SIGTERM" );
     quit();
     sigterm_set->cancel();
   });

   io_serv->run();

   writer( "shutting down..." );

   shutdown(); /// perform synchronous shutdown
}

void application::write_default_config(const bfs::path& cfg_file) {
   if(!bfs::exists(cfg_file.parent_path()))
      bfs::create_directories(cfg_file.parent_path());

   std::ofstream out_cfg( bfs::path(cfg_file).make_preferred().string());
   for(const boost::shared_ptr<bpo::option_description> od : my->_cfg_options.options())
   {
      if(!od->description().empty())
         out_cfg << "# " << od->description() << "\n";
      boost::any store;
      if(!od->semantic()->apply_default(store))
         out_cfg << "# " << od->long_name() << " = \n";
      else
      {
         auto example = od->format_parameter();
         if( example.empty() )
         {
            // This is a boolean switch
            out_cfg << od->long_name() << " = " << "false\n";
         }
         else if( example.length() <= 7 )
         {
            // The string is formatted "arg"
            out_cfg << "# " << od->long_name() << " = \n";
         }
         else
         {
            // The string is formatted "arg (=<interesting part>)"
            example.erase(0, 6);
            example.erase(example.length()-1);
            out_cfg << od->long_name() << " = " << example << "\n";
         }
      }
      out_cfg << "\n";
   }
   out_cfg.close();
}

abstract_plugin* application::find_plugin( const string& name )const
{
   auto itr = plugins.find( name );

   if( itr == plugins.end() )
   {
      return nullptr;
   }

   return itr->second.get();
}

abstract_plugin& application::get_plugin(const string& name)const {
   auto ptr = find_plugin(name);
   if(!ptr)
      BOOST_THROW_EXCEPTION(std::runtime_error("unable to find plugin: " + name));
   return *ptr;
}

bfs::path application::data_dir()const
{
   return my->_data_dir;
}

void application::add_program_options( const options_description& cli, const options_description& cfg )
{
   my->_app_options.add( cli );
   my->_app_options.add( cfg );
   my->_cfg_options.add( cfg );
}

const variables_map& application::get_args() const
{
   return my->_args;
}

void application::for_each_plugin( std::function< void(const abstract_plugin&) > cb ) const
{
   for( auto& p : plugins )
   {
      cb( *(p.second) );
   }
}

} /// namespace appbase
