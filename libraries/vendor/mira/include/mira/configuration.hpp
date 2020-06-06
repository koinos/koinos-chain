#pragma once
#include <boost/any.hpp>

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>

// Fwd declaration
namespace rocksdb { struct Options; }

namespace mira
{

struct mira_config_error final : std::exception
{
   private:
      std::string _msg;

   public:
      mira_config_error( std::string&& msg );
      ~mira_config_error() override;

      const char* what() const noexcept override;
};

class configuration
{
   configuration() = delete;
public:
   static ::rocksdb::Options get_options( const boost::any& cfg, std::string type_name );
   static bool gather_statistics( const boost::any& cfg );
   static size_t get_object_count( const boost::any& cfg );
};

} // mira
